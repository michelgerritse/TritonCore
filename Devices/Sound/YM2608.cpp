/*
 _____    _ _            ___
|_   _| _(_) |_ ___ _ _ / __|___ _ _ ___
  | || '_| |  _/ _ \ ' \ (__/ _ \ '_/ -_)
  |_||_| |_|\__\___/_||_\___\___/_| \___|

Copyright © 2023, Michel Gerritse
All rights reserved.

This source code is available under the BSD-3-Clause license.
See LICENSE.txt in the root directory of this source tree.

*/
#include "YM2608.h"
#include "YM_RSS.h"
#include "ADPCM.h"

#define VGM_WORKAROUND /* Workaround for VGM files */

/*
	Yamaha YM2608 (OPNA)

	- 6 FM Channels
	- 4 Operators per channel
	- 2 Timers (Timer A and B)
	- 1 Shared LFO unit for PM and AM
	- 3CH mode (individual frequency settings for the channel 3 operators)
	- CSM mode (Timer A generated key-on events)
	- SCH mode (Off = backwards compatibility with OPN, On = OPNA mode)
	- SSG-EG envelope modes
	- RSS unit (ADPCM-A rhythm channels)
	- ADPCM-B channel
	- SSG unit

	Needs validation:
	- The attenuation levels of the RSS instruments are 4.2 fixed point according to the manual, I think it is 3.3
	        |  D5  |  D4  |  D3  |  D2  |  D1  |  D0  |
		dB  |  24  |  12  |   6  |   3  | 1.5  | 0.75 |

		vs.

	        |  D5  |  D4  |  D3  |  D2  |  D1  |  D0  |
		dB  |  12  |   6  |   3  | 1.5  | 0.75 |0.375 |

	Needs fixing:
	- Currently using a simplified OPN generation method. No 24-stage pipeline yet

	Not implemented:
	 - Interrupts
	 - SSG IO ports
	 - ADPCM-B encoding from CPU or memory
	 - ADPCM-B decoding from CPU
	 - ADPCM-B memory read/write to/from CPU
	 - OPN / SSG prescalers. Code is there but disabled due to my underlying sound engine not supporting sample rates above 200kHz
*/

/* Device ID */
#define DEVICEID		0x01

/* Status / Control / IRQ register bits */
#define FLAG_TIMERA		0x01	/* Timer A overflow			*/
#define FLAG_TIMERB		0x02	/* Timer B overflow			*/
#define FLAG_EOS		0x04	/* ADPCM-B end of sample	*/
#define FLAG_BRDY		0x08	/* ADPCM-B bus ready		*/
#define FLAG_ZERO		0x10	/* ADPCM-B zero signal		*/
#define FLAG_PCMBUSY	0x20	/* ADPCM-B busy				*/
#define FLAG_BUSY		0x80	/* Register loading			*/

/* ADPCM-B control register 1 bits */
#define CTRL1_RESET		0x01
#define CTRL1_SPOFF		0x08
#define CTRL1_REPEAT	0x10
#define CTRL1_MEMDATA	0x20
#define CTRL1_REC		0x40
#define CTRL1_START		0x80

/* ADPCM-B control register 2 bits */
#define CTRL2_ROM		0x01
#define CTRL2_RAMTYPE	0x02
#define CTRL2_ADDA		0x04
#define CTRL2_SAMPLE	0x08
#define CTRL2_RCH		0x40
#define CTRL2_LCH		0x80

/* Audio output enumeration */
enum AudioOut
{
	SSG = 0,
	OPN,
};

/* Slot naming */
enum SlotName
{
	S1 = 0, S2, S3, S4
};

/* Channel naming */
enum ChannelName
{
	CH1 = 0, CH2, CH3, CH4, CH5, CH6
};

/* Name to Slot ID */
#define O(c, s) { (c << 2) + s}

/* Envelope phases */
enum ADSR : uint32_t
{
	Attack = 0,
	Decay,
	Sustain,
	Release
};

YM2608::YM2608(uint32_t ClockSpeed) :
	m_ClockSpeed(ClockSpeed),
	m_PreScalerOPN(6),
	m_PreScalerSSG(4)
{
	YM::OPN::BuildTables();
	YM::ADPCMA::InitDecoder();

	/* Initialize instrument data only once */
	for (auto i = 0; i < 6; i++)
	{
		m_ADPCMA.Channel[i].Start	= YM::RSS::InstrumentOffsets[(i * 2) + 0];
		m_ADPCMA.Channel[i].End	= YM::RSS::InstrumentOffsets[(i * 2) + 1];
	}

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* YM2608::GetDeviceName()
{
	return L"Yamaha YM2608";
}

void YM2608::Reset(ResetType Type)
{
	m_CyclesToDoSSG = 0;
	m_CyclesToDoOPN = 0;

	/* Reset prescalers */
	//m_PreScalerOPN = 6;
	//m_PreScalerSSG = 4;

	m_ClockADPCMA = 24 * 6 * 3;
	m_ClockADPCMB = 24 * 6;

	/* Reset latches */
	m_AddressLatch = 0;

	/* Reset SSG unit */
	memset(&m_SSG, 0, sizeof(m_SSG));

	/* Default SSG noise state */
	m_SSG.Noise.LFSR = 1 << (17 - 1);

	/* Default SSG envelope state */
	m_SSG.Envelope.Counter = 0;
	m_SSG.Envelope.Period.u32 = 0;
	m_SSG.Envelope.Amplitude = AY::Amplitude32[31];
	m_SSG.Envelope.Step = 31;
	m_SSG.Envelope.StepDec = 1;
	m_SSG.Envelope.Hld = 1;
	m_SSG.Envelope.Alt = 31;
	m_SSG.Envelope.Inv = 0;

	/* Reset OPN unit */
	memset(&m_OPN, 0, sizeof(m_OPN));

	/* Default general register state */
	m_OPN.Status	= 0;
	m_OPN.FlagCtrl	= FLAG_ZERO | FLAG_BRDY | FLAG_EOS;
	m_OPN.IrqEnable	= FLAG_ZERO | FLAG_BRDY | FLAG_EOS | FLAG_TIMERB | FLAG_TIMERA;
	m_OPN.LFO.Period = YM::OPN::LfoPeriod[0];

	/* Default operator register state */
	for (auto& Slot : m_OPN.Slot)
	{
		Slot.Multi = 1; /* x0.5 */
		Slot.EgPhase = ADSR::Release;
		Slot.EgLevel = 0x3FF;
	}

	/* Default channel register state */
	for (auto& Channel : m_OPN.Channel)
	{
		/* All channels are ON by default for OPN compatibility */
		Channel.MaskL = ~0;
		Channel.MaskR = ~0;
	}

	/* Reset RSS unit */
	m_ADPCMA.TotalLevel = 0x3F;
	m_RhythmChannels = 6;

	for (auto& Channel : m_ADPCMA.Channel)
	{
		Channel.KeyOn = 0;
		Channel.OutL = 0;
		Channel.OutR = 0;
		Channel.Level = 0x1F;
		Channel.MaskL = ~0;
		Channel.MaskR = ~0;
	}

	/* Reset ADPCM-B unit */
	memset(&m_ADPCMB, 0, sizeof(m_ADPCMB));
	m_ADPCMB.AddrShift = 2;
	m_ADPCMB.Limit.u32 = 0xFFFF;

	/* Reset ADPCM-B memory */
	if (Type == ResetType::PowerOnDefaults)
	{
		m_MemoryADPCMB.fill(0);
	}
}

void YM2608::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
	if (Command & 0x100) /* Port 1 */
	{
		Write(0x02, Command & 0xFF);
		Write(0x03, Value);
	}
	else /* Port 0 */
	{
		Write(0x00, Command & 0xFF);
		Write(0x01, Value);
	}
}

bool YM2608::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	switch (OutputNr)
	{
	case AudioOut::SSG: /* SSG - Analog Out */
		Desc.SampleRate = m_ClockSpeed / (16 * m_PreScalerSSG);
		Desc.SampleFormat = 0;
		Desc.Channels = 1;
		Desc.ChannelMask = SPEAKER_FRONT_CENTER;
		Desc.Description = L"Analog Out";
		break;

	case AudioOut::OPN: /* FM */
		Desc.SampleRate = m_ClockSpeed / (24 * m_PreScalerOPN);
		Desc.SampleFormat = 0;
		Desc.Channels = 2;
		Desc.ChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		Desc.Description = L"FM + ADPCM";
		break;

	default:
		return false;
	}

	return true;
}

void YM2608::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YM2608::GetClockSpeed()
{
	return m_ClockSpeed;
}

uint32_t YM2608::Read(int32_t Address)
{
	/* 2-bit address bus (A0 - A1) */
	Address &= 0x03;

	switch (Address)
	{
	case 0x00: /* Read status 0 */
		return m_OPN.Status & (FLAG_BUSY | FLAG_TIMERB | FLAG_TIMERA); /* For compatibility with OPN (YM2203) */

	case 0x01: /* Read SSG registers / Device ID */
		if (m_AddressLatch < 0x10)
		{
			return m_SSG.Register[m_AddressLatch];
		}
		else if (m_AddressLatch == 0xFF)
		{
			return DEVICEID;
		}

		return 0;

	case 0x02: /* Read status 1 */
		return m_OPN.Status;

	case 0x03: /* Read ADPCM_B data*/
		/* Not implemented yet */
		return 0;
	}

	return 0;
}

void YM2608::Write(uint32_t Address, uint32_t Data)
{
	/* 8-bit data bus (D0 - D7) */
	Data &= 0xFF;

	/* 2-bit address bus (A0 - A1) */
	Address &= 0x03;

	switch (Address)
	{
	case 0x00: /* Port 0 addressing mode */
	case 0x02: /* Port 1 addressing mode */
		m_AddressLatch = Data;
		break;

	case 0x01: /* Port 0 data write mode */
		switch (m_AddressLatch & 0xF0)
		{
		case 0x00: /* Write SSG data (0x00 - 0x0F) */
			WriteSSG(m_AddressLatch, Data);
			break;

		case 0x10: /* Write RSS data (0x10 - 0x1F) */
			WriteRSS(m_AddressLatch, Data);
			break;

		case 0x20: /* Write OPN mode data (0x20 - 0x2F) */
			WriteMode(m_AddressLatch, Data);
			break;

		default: /* Write OPN FM data (0x30 - 0xB6) */
			WriteFM(m_AddressLatch, 0, Data);
			break;
		}
		break;

	case 0x03:/* Port 1 data write mode */
		switch (m_AddressLatch & 0xF0)
		{
		case 0x00: /* Write ADPCM-B data (0x00 - 0x0F) */
		case 0x10: /* Flag Control / Unused (0x10 - 0x1F) */
			WriteADPCMB(m_AddressLatch, Data);
			break;

		case 0x20: /* Unused (0x20 - 0x2F) */
			break;

		default: /* Write OPN FM data (0x30 - 0xB6) */
			WriteFM(m_AddressLatch, 1, Data);
			break;
		}
		break;
	}
}

void YM2608::WriteSSG(uint8_t Address, uint8_t Data)
{
	Address &= 0x0F;
	m_SSG.Register[Address] = Data;

	Data &= AY::Mask[Address]; /* Mask unused bits after storing */

	switch (Address)
	{
	case 0x00: /* Channel A Tone Period (Fine Tune) */
		m_SSG.Tone[0].Period.u8ll = Data;
		break;

	case 0x01: /* Channel A Tone Period (Coarse Tune) */
		m_SSG.Tone[0].Period.u8lh = Data;
		break;

	case 0x02: /* Channel B Tone Period (Fine Tune) */
		m_SSG.Tone[1].Period.u8ll = Data;
		break;

	case 0x03: /* Channel B Tone Period (Coarse Tune) */
		m_SSG.Tone[1].Period.u8lh = Data;
		break;

	case 0x04: /* Channel C Tone Period (Fine Tune) */
		m_SSG.Tone[2].Period.u8ll = Data;
		break;

	case 0x05: /* Channel C Tone Period (Coarse Tune) */
		m_SSG.Tone[2].Period.u8lh = Data;
		break;

	case 0x06: /* Noise Period */
		m_SSG.Noise.Period = Data;
		break;

	case 0x07: /* Mixer Control - I/O Enable */
		m_SSG.Tone[0].ToneDisable = (Data >> 0) & 1;
		m_SSG.Tone[1].ToneDisable = (Data >> 1) & 1;
		m_SSG.Tone[2].ToneDisable = (Data >> 2) & 1;

		m_SSG.Tone[0].NoiseDisable = (Data >> 3) & 1;
		m_SSG.Tone[1].NoiseDisable = (Data >> 4) & 1;
		m_SSG.Tone[2].NoiseDisable = (Data >> 5) & 1;
		break;

	case 0x08: /* Channel A Amplitude Control */
		m_SSG.Tone[0].Amplitude = AY::Amplitude32[AY::MapLvl4to5[Data & 0x0F]];
		m_SSG.Tone[0].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x09: /* Channel B Amplitude Control */
		m_SSG.Tone[1].Amplitude = AY::Amplitude32[AY::MapLvl4to5[Data & 0x0F]];
		m_SSG.Tone[1].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x0A: /* Channel C Amplitude Control */
		m_SSG.Tone[2].Amplitude = AY::Amplitude32[AY::MapLvl4to5[Data & 0x0F]];
		m_SSG.Tone[2].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x0B: /* Envelope Period (Fine Tune) */
		m_SSG.Envelope.Period.u8ll = Data;
		break;

	case 0x0C: /* Envelope Period (Coarse Tune) */
		m_SSG.Envelope.Period.u8lh = Data;
		break;

	case 0x0D: /* Envelope Shape / Cycle Control */
		m_SSG.Envelope.Counter = 0;
		m_SSG.Envelope.Step = 31;
		m_SSG.Envelope.StepDec = 1;

		/* If attacking, apply output inversion */
		m_SSG.Envelope.Inv = (Data & 0x04) ? 31 : 0;

		if (Data & 0x08) /* Continuous cycles */
		{
			m_SSG.Envelope.Hld = Data & 0x01;

			if (m_SSG.Envelope.Hld)
				m_SSG.Envelope.Alt = (Data & 0x02) ? 0 : 31;
			else
				m_SSG.Envelope.Alt = (Data & 0x02) ? 31 : 0;
		}
		else /* Single cycle */
		{
			m_SSG.Envelope.Hld = 1;
			m_SSG.Envelope.Alt = m_SSG.Envelope.Inv ^ 31;
		}

		/* Set initial ouput volume */
		m_SSG.Envelope.Amplitude = AY::Amplitude32[m_SSG.Envelope.Step ^ m_SSG.Envelope.Inv];
		break;

	case 0x0E: /* I/O Port A Data Store */
		/* Not implemented */
		break;

	case 0x0F: /* I/O Port B Data Store */
		/* Not implemented */
		break;
	}
}

void YM2608::WriteRSS(uint8_t Address, uint8_t Data)
{	
	switch (Address & 0x0F)
	{
	case 0x00: /* Dump / Rhythm Key On */
	{
		/* According to YM2608 manual page 41:
		   DM = 0: The channels specified by B0-5 are keyed on
		   DM = 1: The channels specified by B0-5 are keyed off */

		uint8_t DM = (Data & 0x80) ? 0 : 1;

		for (auto i = 0; i < 6; i++)
		{
			if ((Data >> i) & 0x01)
			{
				auto& Channel = m_ADPCMA.Channel[i];
				
				if (DM) /* Key On */
				{
					/* Reload start address */
					Channel.Addr = Channel.Start.u32;

					/* Reset decoder state */
					Channel.Step = 0;
					Channel.Signal = 0;
					Channel.NibbleShift = 4; /* Start at high nibble */
				}

				Channel.KeyOn = DM;
			}
		}
		break;
	}

	case 0x01: /* Rhythm Total Level */
		/* -47.5dB - 0dB (64 steps, 0.75dB resolution) */
		m_ADPCMA.TotalLevel = ~Data & 0x3F;
		break;

	case 0x02: /* LSI Test */
		/* Not implemented */
		break;

	case 0x08: /* Output selection / Instrument level (BD) */
	case 0x09: /* Output selection / Instrument level (SD) */
	case 0x0A: /* Output selection / Instrument level (TOP) */
	case 0x0B: /* Output selection / Instrument level (HH) */
	case 0x0C: /* Output selection / Instrument level (TOM) */
	case 0x0D: /* Output selection / Instrument level (RIM) */
	{
		auto Instr = Address & 0x07;
		if (Instr >= 6) break; /* Keeps the compiler happy: warnings about m_ADPCMA.Channel[Address & 0x07] */

		auto& Channel = m_ADPCMA.Channel[Instr];
		
		/* Output selection is implemented as a mask */
		Channel.MaskL = (Data & 0x80) ? ~0 : 0;
		Channel.MaskR = (Data & 0x40) ? ~0 : 0;

		/* -23.25dB - 0dB (32 steps, 0.75dB resolution) */
		Channel.Level = ~Data & 0x1F;
		break;
	}

	default: /* Unused */
		break;
	}
}

void YM2608::WriteADPCMB(uint8_t Address, uint8_t Data)
{
	switch (Address & 0x1F)
	{
	case 0x00: /* Control register 1 */
		m_ADPCMB.Ctrl1 = Data;

		if (m_ADPCMB.Ctrl1 & CTRL1_RESET)
		{
			ClearStatusFlags(FLAG_PCMBUSY | FLAG_ZERO | FLAG_EOS);
			SetStatusFlags(FLAG_BRDY);
		}

		/* Note: Only ADPCM-B decoding from memory supported */
		if ((m_ADPCMB.Ctrl1 & (CTRL1_START | CTRL1_REC | CTRL1_MEMDATA)) == (CTRL1_START | CTRL1_MEMDATA))
		{
			ClearStatusFlags(FLAG_ZERO | FLAG_BRDY | FLAG_EOS);
			SetStatusFlags(FLAG_PCMBUSY);
			
			m_ADPCMB.AddrCount = m_ADPCMB.Start.u32 << m_ADPCMB.AddrShift;
			m_ADPCMB.AddrDelta = 0;

			m_ADPCMB.SignalT1 = 0;
			m_ADPCMB.SignalT0 = 0;
			m_ADPCMB.Step = 127;
			m_ADPCMB.NibbleShift = 4;
		}
		break;

	case 0x01: /* Control register 2 */
		if ((Data & (CTRL2_RAMTYPE | CTRL2_ROM)) == 0)
			m_ADPCMB.AddrShift = 2; /* DRAM 1-bit access: 4-bytes aligned */
		else
			m_ADPCMB.AddrShift = 5; /* ROM/DRAM 8-bit access: 32-bytes aligned */

		/* Output selection is implemented as a mask */
		m_ADPCMB.MaskL = (Data & CTRL2_LCH) ? ~0 : 0;
		m_ADPCMB.MaskR = (Data & CTRL2_RCH) ? ~0 : 0;
		break;

	case 0x02: /* Start address (L) */
		m_ADPCMB.Start.u8ll = Data;
		break;

	case 0x03: /* Start address (H) */
		m_ADPCMB.Start.u8lh = Data;
		break;

	case 0x04: /* Stop address (L) */
		m_ADPCMB.Stop.u8ll = Data;
		break;

	case 0x05: /* Stop address (H) */
		m_ADPCMB.Stop.u8lh = Data;
		break;

	case 0x06: /* Prescale (L) */
		m_ADPCMB.Prescale.u8ll = Data;
		break;

	case 0x07: /* Prescale (H) */
		m_ADPCMB.Prescale.u8lh = Data & 0x07;
		break;

	case 0x08: /* ADPCM data */
		break;

	case 0x09: /* Delta-N (L) */
		m_ADPCMB.DeltaN.u8ll = Data;
		break;

	case 0x0A: /* Delta-N (H) */
		m_ADPCMB.DeltaN.u8lh = Data;
		break;

	case 0x0B: /* Level control */
		m_ADPCMB.LevelCtrl = Data;
		break;

	case 0x0C: /* Limit address (L) */
		m_ADPCMB.Limit.u8ll = Data;
		break;

	case 0x0D: /* Limit address (H) */
		m_ADPCMB.Limit.u8lh = Data;
		break;

	case 0x0E: /* DAC data */
		break;

	case 0x0F: /* PCM data */
		break;

	case 0x10: /* Flag control */
		if (Data & 0x80) /* IRQ Reset */
		{
			/* TODO: Clear interrupt line */
		}
		else
		{
			m_OPN.FlagCtrl = Data & 0x1F;
		}
		break;

	default: /* Unused (0x11 - 0x1F) */
		break;
	}
}

void YM2608::WriteMode(uint8_t Address, uint8_t Data)
{
	switch (Address) /* 0x20 - 0x2F */
	{
	case 0x20: /* Not used */
		break;
	
	case 0x21: /* LSI Test */
		/* Not implemented */
		break;

	case 0x22: /* LFO Control */
		m_OPN.LFO.Enable = (Data & 0x08) ? ~0 : 0; /* Note: implemented as a mask */
		m_OPN.LFO.Period = YM::OPN::LfoPeriod[Data & 0x07];
		break;

	case 0x23: /* Not used */
		break;

	case 0x24: /* Timer A [9:2] */
		m_OPN.TimerA.Period &= 0x03;
		m_OPN.TimerA.Period |= (Data << 2);
		break;

	case 0x25: /* Timer A [1:0] */
		m_OPN.TimerA.Period &= 0x3FC;
		m_OPN.TimerA.Period |= (Data & 0x03);
		break;

	case 0x26: /* Timer B */
		m_OPN.TimerB.Period = Data;
		break;

	case 0x27: /* 3CH mode / Timer control */
	{
		/* Timer A and B start / stop */
		auto StartA = (Data >> 0) & 0x01;
		auto StartB = (Data >> 1) & 0x01;
		
		if (m_OPN.TimerA.Load ^ StartA)
		{
			m_OPN.TimerA.Load = StartA;
			m_OPN.TimerA.Counter = 1024 - m_OPN.TimerA.Period;
		}

		if (m_OPN.TimerB.Load ^ StartB)
		{
			m_OPN.TimerB.Load = StartB;
			m_OPN.TimerB.Counter = (256 - m_OPN.TimerB.Period) << 4; /* Note: period x16 to sync with Timer A */
		}

		/* Timer A/B enable */
		m_OPN.TimerA.Enable = (Data >> 2) & 0x01;
		m_OPN.TimerB.Enable = (Data >> 3) & 0x01;

		/* Timer A/B overflow flag reset */
		if (Data & 0x10) ClearStatusFlags(FLAG_TIMERA);
		if (Data & 0x20) ClearStatusFlags(FLAG_TIMERB);

		/* 3CH / CSM mode */
		m_OPN.Mode3CH = ((Data & 0xC0) != 0x00) ? 1 : 0;
		m_OPN.ModeCSM = ((Data & 0xC0) == 0x80) ? 1 : 0;
	}
		break;

	case 0x28: /* Key On/Off */
	{
		if ((Data & 0x03) == 0x03) break; /* Invalid channel */
		if ((Data & 0x04) && (m_OPN.ModeSCH == 0)) break; /* We're in OPN compatibility mode */
		
		uint32_t ChannelId = ((Data & 0x03) + ((Data & 0x04) ? 3 : 0)) << 2;

		m_OPN.Slot[ChannelId + S1].KeyLatch = (Data >> 4) & 0x01;
		m_OPN.Slot[ChannelId + S2].KeyLatch = (Data >> 5) & 0x01;
		m_OPN.Slot[ChannelId + S3].KeyLatch = (Data >> 6) & 0x01;
		m_OPN.Slot[ChannelId + S4].KeyLatch = (Data >> 7) & 0x01;

#ifdef VGM_WORKAROUND
		/*	Note: This is a work-around as key events should not be procesed here.
			Some VGM files write consecutive key-on / key-off data without a render update in between.
			This causes latched key data, which has not yet been processed, to be overwritten
		*/
		ProcessKeyEvent(ChannelId + S1);
		ProcessKeyEvent(ChannelId + S2);
		ProcessKeyEvent(ChannelId + S3);
		ProcessKeyEvent(ChannelId + S4);
#endif // VGM_WORKAROUND
		break;
	}

	case 0x29: /* SCH / IRQ Enable */
		m_OPN.ModeSCH   = (Data >> 7) & 0x01;
		m_OPN.IrqEnable = (Data >> 0) & 0x1F;
		break;

	case 0x2A: /* Not used */
		break;

	case 0x2B: /* Not used */
		break;

	case 0x2C: /* Not used */
		break;

	case 0x2D: /* Prescaler selection (/6) */
		m_PreScalerOPN = 6;
		m_PreScalerSSG = 4;
		break;

	case 0x2E: /* Prescaler selection (/3) */
		if (m_PreScalerOPN == 6)
		{
			m_PreScalerOPN = 3;
			m_PreScalerSSG = 2;
		}
		break;

	case 0x2F: /* Prescaler selection (/2) */
		m_PreScalerOPN = 2;
		m_PreScalerSSG = 1;
		break;
	}
}

void YM2608::WriteFM(uint8_t Address, uint8_t Port, uint8_t Data)
{
	/* Slot address mapping: S1 - S3 - S2 - S4 */
	static const int32_t SlotMap[2][16] =
	{
		{
			/* Port 0: Channel 1 - 3 */
			O(CH1,S1), O(CH2,S1), O(CH3,S1), -1, O(CH1,S3), O(CH2,S3), O(CH3,S3), -1,
			O(CH1,S2), O(CH2,S2), O(CH3,S2), -1, O(CH1,S4), O(CH2,S4), O(CH3,S4), -1
		},
		{
			/* Port 1: Channel 4 - 6 */
			O(CH4,S1), O(CH5,S1), O(CH6,S1), -1, O(CH4,S3), O(CH5,S3), O(CH6,S3), -1,
			O(CH4,S2), O(CH5,S2), O(CH6,S2), -1, O(CH4,S4), O(CH5,S4), O(CH6,S4), -1
		}
	};

	int32_t SlotId = SlotMap[Port][Address & 0x0F];
	if (SlotId == -1) return;

	if (Address < 0xA0) /* Slot register map (0x30 - 0x9F) */
	{
		auto& Slot = m_OPN.Slot[SlotId];

		switch (Address & 0xF0)
		{
		case 0x30: /* Detune / Multiply */
			Slot.Detune = (Data >> 4) & 0x07;
			Slot.Multi = (Data & 0x0F) << 1;
			if (Slot.Multi == 0) Slot.Multi = 1;
			break;

		case 0x40: /* Total Level */
			Slot.TotalLevel = (Data & 0x7F) << 3;
			break;

		case 0x50: /* Key Scale / Attack Rate */
			Slot.KeyScale = (Data >> 6);
			Slot.EgRate[ADSR::Attack] = Data & 0x1F;
			break;

		case 0x60: /* Decay Rate / AM On */
			Slot.AmOn = (Data & 0x80) ? ~0 : 0; /* Note: AM On/Off is implemented as a mask */
			Slot.EgRate[ADSR::Decay] = Data & 0x1F;
			break;

		case 0x70: /* Sustain Rate */
			Slot.EgRate[ADSR::Sustain] = Data & 0x1F;
			break;

		case 0x80: /* Sustain Level / Release Rate */
			/* If all SL bits are set, SL is 93dB. See YM2608 manual page 28 */
			Slot.SustainLvl = (Data >> 4) & 0x0F;
			Slot.SustainLvl |= (Slot.SustainLvl + 1) & 0x10;
			Slot.SustainLvl <<= 5;

			/* Map RR from 4 to 5 bits, with LSB always set to 1 */
			Slot.EgRate[ADSR::Release] = ((Data & 0x0F) << 1) | 0x01;
			break;

		case 0x90: /* SSG-EG Envelope Control */
			Slot.SsgEnable = (Data >> 3) & 0x01;
			Slot.SsgEgInv = (Data >> 2) & 0x01;
			Slot.SsgEgAlt = (Data >> 1) & 0x01;
			Slot.SsgEgHld = (Data >> 0) & 0x01;
			break;
		}
	}
	else /* Channel register map (0xA0 - 0xB6) */
	{
		auto& Chan = m_OPN.Channel[SlotId >> 2];

		switch (Address & 0xFC)
		{
		case 0xA0: /* F-Num 1 */
			Chan.FNum = ((m_OPN.FNumLatch & 0x07) << 8) | Data;
			Chan.Block = m_OPN.FNumLatch >> 3;
			Chan.KeyCode = (Chan.Block << 2) | YM::OPN::Note[Chan.FNum >> 7];
			break;

		case 0xA4: /* F-Num 2 / Block Latch */
			m_OPN.FNumLatch = Data & 0x3F;
			break;

		case 0xA8: /* 3 Ch-3 F-Num  */
			if (Port == 0)
			{
				/* Slot order for 3CH mode */
				if (Address == 0xA9)
				{
					m_OPN.FNum3CH[S1] = ((m_OPN.FNumLatch3CH & 0x07) << 8) | Data;
					m_OPN.Block3CH[S1] = m_OPN.FNumLatch3CH >> 3;
					m_OPN.KeyCode3CH[S1] = (m_OPN.Block3CH[S1] << 2) | YM::OPN::Note[m_OPN.FNum3CH[S1] >> 7];
				}
				else if (Address == 0xA8)
				{
					m_OPN.FNum3CH[S3] = ((m_OPN.FNumLatch3CH & 0x07) << 8) | Data;
					m_OPN.Block3CH[S3] = m_OPN.FNumLatch3CH >> 3;
					m_OPN.KeyCode3CH[S3] = (m_OPN.Block3CH[S3] << 2) | YM::OPN::Note[m_OPN.FNum3CH[S3] >> 7];
				}
				else /* 0xAA */
				{
					m_OPN.FNum3CH[S2] = ((m_OPN.FNumLatch3CH & 0x07) << 8) | Data;
					m_OPN.Block3CH[S2] = m_OPN.FNumLatch3CH >> 3;
					m_OPN.KeyCode3CH[S2] = (m_OPN.Block3CH[S2] << 2) | YM::OPN::Note[m_OPN.FNum3CH[S2] >> 7];
				}
			}
			break;

		case 0xAC: /* 3 Ch-3 F-Num / Block Latch */
			if (Port == 0) m_OPN.FNumLatch3CH = Data & 0x3F;
			break;

		case 0xB0: /* Feedback / Connection */
			Chan.FB = (Data >> 3) & 0x07;
			Chan.Algo = Data & 0x07;
			break;

		case 0xB4: /* PMS / AMS / Panning */
			Chan.MaskL = (Data & 0x80) ? ~0 : 0;
			Chan.MaskR = (Data & 0x40) ? ~0 : 0;
			Chan.AMS = (Data >> 4) & 0x03;
			Chan.PMS = Data & 0x07;
			break;
		}
	}
}

void YM2608::SetStatusFlags(uint8_t Flags)
{
	m_OPN.Status |= Flags & ~m_OPN.FlagCtrl;

	if (m_OPN.Status & m_OPN.IrqEnable & Flags)
	{
		/* TODO: Set interrupt line */
	}
}

void YM2608::ClearStatusFlags(uint8_t Flags)
{
	m_OPN.Status &= ~Flags;
}

void YM2608::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	UpdateSSG(ClockCycles, OutBuffer);
	UpdateOPN(ClockCycles, OutBuffer);
}

void YM2608::CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	switch (MemoryID)
	{
	case YM::OPN::Memory::ADPCMB:
		if ((Offset + Size) > m_MemoryADPCMB.size()) break;
		memcpy(m_MemoryADPCMB.data() + Offset, Data, Size);
		break;

	default:
		break;
	}
}

void YM2608::CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	/* No specialized implementation needed */
	CopyToMemory(MemoryID, Offset, Data, Size);
}

void YM2608::UpdateSSG(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDoSSG;
	uint32_t Samples = TotalCycles / (16 * m_PreScalerSSG);
	m_CyclesToDoSSG = TotalCycles % (16 * m_PreScalerSSG);

	int16_t Out;
	uint32_t Mask;

	while (Samples-- != 0)
	{
		Out = 0;

		/* Update envelope generator */
		if ((m_SSG.Envelope.Counter += 2) >= m_SSG.Envelope.Period.u32) //FIXME: should be += 1
		{
			/* Reset counter */
			m_SSG.Envelope.Counter = 0;

			/* Count down step counter (31 -> 0) */
			m_SSG.Envelope.Step -= m_SSG.Envelope.StepDec;

			if (m_SSG.Envelope.Step & 32) /* Envelope cycle completed */
			{
				/* Restart cycle */
				m_SSG.Envelope.Step = 31;

				/* Stop counting (if needed) */
				m_SSG.Envelope.StepDec = m_SSG.Envelope.Hld ^ 1;

				/* Toggle output inversion */
				m_SSG.Envelope.Inv ^= m_SSG.Envelope.Alt;
			}

			/* Apply output inversion and lookup amplitude */
			m_SSG.Envelope.Amplitude = AY::Amplitude32[m_SSG.Envelope.Step ^ m_SSG.Envelope.Inv];
		}

		/* Update noise generator */
		if (m_SSG.Noise.Prescaler ^= 1)
		{
			if ((m_SSG.Noise.Counter += 2) >= m_SSG.Noise.Period) //FIXME: should be += 1
			{
				/* Reset counter */
				m_SSG.Noise.Counter = 0;

				/* Update output flag */
				m_SSG.Noise.Output = m_SSG.Noise.LFSR & 1;

				/* Tap bits 3 and 0 (XOR feedback) */
				uint32_t Seed = ((m_SSG.Noise.LFSR >> 3) ^ (m_SSG.Noise.LFSR >> 0)) & 1;

				/* Shift LFSR and apply seed (17-bit wide) */
				m_SSG.Noise.LFSR = (m_SSG.Noise.LFSR >> 1) | (Seed << 16);
			}
		}

		/* Update, mix and buffer tone generators */
		for (auto i = 0; i < 3; i++)
		{
			auto& Tone = m_SSG.Tone[i];

			if ((Tone.Counter += 2) >= Tone.Period.u32) //FIXME: should be += 1
			{
				/* Reset counter */
				Tone.Counter = 0;

				/* Toggle output flag */
				Tone.Output ^= 1;
			}

			/* Mix tone and noise (implemented as a mask) */
			Mask = ~(((Tone.Output | Tone.ToneDisable) & (m_SSG.Noise.Output | Tone.NoiseDisable)) - 1);

			/* Amplitude control */
			Out += (Tone.AmpCtrl ? m_SSG.Envelope.Amplitude : Tone.Amplitude) & Mask;
		}

		/* 16-bit output */
		OutBuffer[AudioOut::SSG]->WriteSampleS16(Out >> 1);
	}
}

void YM2608::UpdateOPN(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	static const uint32_t SlotOrder[] =
	{
		 O(CH1,S1), O(CH2,S1), O(CH3,S1), O(CH4,S1), O(CH5,S1), O(CH6,S1),
		 O(CH1,S3), O(CH2,S3), O(CH3,S3), O(CH4,S3), O(CH5,S3), O(CH6,S3),
		 O(CH1,S2), O(CH2,S2), O(CH3,S2), O(CH4,S2), O(CH5,S2), O(CH6,S2),
		 O(CH1,S4), O(CH2,S4), O(CH3,S4), O(CH4,S4), O(CH5,S4), O(CH6,S4)
	};

	uint32_t TotalCycles = ClockCycles + m_CyclesToDoOPN;
	uint32_t Samples = TotalCycles / (24 * m_PreScalerOPN);
	m_CyclesToDoOPN = TotalCycles % (24 * m_PreScalerOPN);

	int32_t OutL;
	int32_t OutR;

	while (Samples-- != 0)
	{
		OutL = 0;
		OutR = 0;

		/* Update Timer A, Timer B and LFO */
		UpdateTimers();
		UpdateLFO();

		/* Update envelope clock */
		m_OPN.EgClock = (m_OPN.EgClock + 1) % 3;

		/* Update envelope counter */
		m_OPN.EgCounter = (m_OPN.EgCounter + (m_OPN.EgClock >> 1)) & 0xFFF;

		/* Update slots (operators) */
		for (auto& Slot : SlotOrder)
		{
			PrepareSlot(Slot);
			UpdatePhaseGenerator(Slot);
			UpdateEnvelopeGenerator(Slot);
			UpdateOperatorUnit(Slot);
		}

		UpdateAccumulator(CH1);
		UpdateAccumulator(CH2);
		UpdateAccumulator(CH3);

		OutL += (m_OPN.Channel[CH1].Output & m_OPN.Channel[CH1].MaskL);
		OutR += (m_OPN.Channel[CH1].Output & m_OPN.Channel[CH1].MaskR);
		OutL += (m_OPN.Channel[CH2].Output & m_OPN.Channel[CH2].MaskL);
		OutR += (m_OPN.Channel[CH2].Output & m_OPN.Channel[CH2].MaskR);
		OutL += (m_OPN.Channel[CH3].Output & m_OPN.Channel[CH3].MaskL);
		OutR += (m_OPN.Channel[CH3].Output & m_OPN.Channel[CH3].MaskR);

		if (m_OPN.ModeSCH) /* 6-channel mode enabled */
		{
			UpdateAccumulator(CH4);
			UpdateAccumulator(CH5);
			UpdateAccumulator(CH6);

			OutL += (m_OPN.Channel[CH4].Output & m_OPN.Channel[CH4].MaskL);
			OutR += (m_OPN.Channel[CH4].Output & m_OPN.Channel[CH4].MaskR);
			OutL += (m_OPN.Channel[CH5].Output & m_OPN.Channel[CH5].MaskL);
			OutR += (m_OPN.Channel[CH5].Output & m_OPN.Channel[CH5].MaskR);
			OutL += (m_OPN.Channel[CH6].Output & m_OPN.Channel[CH6].MaskL);
			OutR += (m_OPN.Channel[CH6].Output & m_OPN.Channel[CH6].MaskR);

			/* Limiter (signed 16-bit) */
			OutL = std::clamp(OutL, -32768, 32767);
			OutR = std::clamp(OutR, -32768, 32767);
		}

		/* Update ADPCM-A clock */
		m_ClockADPCMA -= (m_PreScalerOPN * 24);
		if (m_ClockADPCMA <= 0)
		{
			m_ClockADPCMA += (6 * 24 * 3);
			UpdateADPCMA();
		}

		/* Update ADPCM-B clock */
		m_ClockADPCMB -= (m_PreScalerOPN * 24);
		if (m_ClockADPCMB <= 0)
		{
			m_ClockADPCMB += (6 * 24);
			UpdateADPCMB();
		}

		/* Mix FM, ADPCM-A and ADPCM-B */
		OutL += m_ADPCMB.OutL + m_ADPCMA.OutL;
		OutR += m_ADPCMB.OutR + m_ADPCMA.OutR;

		/* 16-bit output */
		OutBuffer[AudioOut::OPN]->WriteSampleS16(OutL >> 1);
		OutBuffer[AudioOut::OPN]->WriteSampleS16(OutR >> 1);
	}
}

void YM2608::UpdateADPCMA()
{	
	/* Update 4 or 6 channels */
	for (uint32_t i = 0; i < m_RhythmChannels; i++)
	{
		auto& Channel = m_ADPCMA.Channel[i];

		if (Channel.KeyOn)
		{
			/* Check for end address */
			if (Channel.Addr > Channel.End.u32)
			{
				Channel.KeyOn = 0;
				Channel.OutL = 0;
				Channel.OutR = 0;
				continue;
			}
				
			/* Read nibble from instrument ROM */
			uint8_t Nibble = (YM::RSS::InstrumentROM[Channel.Addr] >> Channel.NibbleShift) & 0x0F;

			/* Alternate between 1st and 2nd nibble */
			Channel.NibbleShift ^= 4;

			/* Increase nibble counter */
			Channel.Addr += (Channel.NibbleShift >> 2);

			/* Decode ADPCM-A nibble */
			YM::ADPCMA::Decode(Nibble, &Channel.Step, &Channel.Signal);

			/* Attenuation (3.3 to 3.8 fixed point??) */
			uint32_t Attn = std::min(m_ADPCMA.TotalLevel + Channel.Level, 63u) << 5;

			/* dB to linear conversion (13-bit) */
			uint32_t Volume = YM::ExpTable[Attn & 0xFF] >> (Attn >> 8);

			/* Multiply and shift ADPCM-A signal */
			int16_t Sample = (Volume * Channel.Signal) >> 9;

			Channel.OutL = Sample & Channel.MaskL;
			Channel.OutR = Sample & Channel.MaskL;
		}
	}

	/* Alternate between 4 and 6 channels */
	m_RhythmChannels ^= 0x02;

	/* Accumulate samples from all channels */
	m_ADPCMA.OutL = 0;
	m_ADPCMA.OutR = 0;

	for (auto& Channel : m_ADPCMA.Channel)
	{
		m_ADPCMA.OutL += Channel.OutL;
		m_ADPCMA.OutR += Channel.OutR;
	}
}

void YM2608::UpdateADPCMB()
{
	if (m_OPN.Status & FLAG_PCMBUSY)
	{
		/* Add frequency delta (range: 2362 - 65536) */
		m_ADPCMB.AddrDelta.u32 += m_ADPCMB.DeltaN.u32; /* + 1 to have a true 55.5KHz output ??*/

		if (m_ADPCMB.AddrDelta.u16h) /* Moved to a new nibble address (counter overflow) */
		{
			m_ADPCMB.AddrDelta.u16h = 0;

			if ((m_ADPCMB.AddrCount >> m_ADPCMB.AddrShift) == (m_ADPCMB.Stop.u32 + 1))
			{
				if (m_ADPCMB.Ctrl1 & CTRL1_REPEAT) /* Loop */
				{
					m_ADPCMB.AddrCount = m_ADPCMB.Start.u32 << m_ADPCMB.AddrShift;
					m_ADPCMB.AddrDelta = 0;

					m_ADPCMB.SignalT1 = 0;
					m_ADPCMB.SignalT0 = 0;
					m_ADPCMB.Step = 127;
					m_ADPCMB.NibbleShift = 4;
				}
				else /* Don't loop */
				{
					ClearStatusFlags(FLAG_PCMBUSY);
					SetStatusFlags(FLAG_EOS);

					m_ADPCMB.OutL = 0;
					m_ADPCMB.OutR = 0;
					return;
				}
			}

			/* Read nibble from external memory */
			uint8_t Nibble = (m_MemoryADPCMB[m_ADPCMB.AddrCount] >> m_ADPCMB.NibbleShift) & 0x0F;

			/* Alternate between 1st and 2nd nibble */
			m_ADPCMB.NibbleShift ^= 4;

			/* Update address counter */
			m_ADPCMB.AddrCount += (m_ADPCMB.NibbleShift >> 2);

			/* Limit address counter */
			if ((m_ADPCMB.AddrCount >> m_ADPCMB.AddrShift) == (m_ADPCMB.Limit.u32 + 1))
				m_ADPCMB.AddrCount = 0;

			/* Save previous sample */
			m_ADPCMB.SignalT0 = m_ADPCMB.SignalT1;

			/* Decode ADPCM-B nibble */
			YM::ADPCMB::Decode(Nibble, &m_ADPCMB.Step, &m_ADPCMB.SignalT1);
		}

		/* Linear interpolation */
		uint16_t T0 = 0x10000 - m_ADPCMB.AddrDelta.u16l;
		uint16_t T1 = m_ADPCMB.AddrDelta.u16l;
		int16_t Sample = ((T0 * m_ADPCMB.SignalT0) + (T1 * m_ADPCMB.SignalT1)) >> 16;

		/* Level control (256-steps) */
		Sample = (Sample * m_ADPCMB.LevelCtrl) >> 8;

		/* Final output */
		m_ADPCMB.OutL = Sample & m_ADPCMB.MaskL;
		m_ADPCMB.OutR = Sample & m_ADPCMB.MaskR;
	}
}

void YM2608::PrepareSlot(uint32_t SlotId)
{
	uint32_t ChannelId = SlotId >> 2;
	auto& Chan = m_OPN.Channel[ChannelId];
	auto& Slot = m_OPN.Slot[SlotId];

	/* Copy some values for later processing */
	Slot.FNum = Chan.FNum;
	Slot.Block = Chan.Block;
	Slot.KeyCode = Chan.KeyCode;

	if (m_OPN.Mode3CH)
	{
		auto i = SlotId & 3;

		/* Get Block/FNum for channel 3: S1-S2-S3 */
		if ((ChannelId == CH3) && (i != S4))
		{
			Slot.FNum = m_OPN.FNum3CH[i];
			Slot.Block = m_OPN.Block3CH[i];
			Slot.KeyCode = m_OPN.KeyCode3CH[i];
		}
	}
}

void YM2608::UpdatePhaseGenerator(uint32_t SlotId)
{
	auto& Chan = m_OPN.Channel[SlotId >> 2];
	auto& Slot = m_OPN.Slot[SlotId];

	uint32_t FNum = Slot.FNum << 1; /* 11 to 12-bit */

	/* LFO frequency modulation (12-bit result) */
	FNum = (FNum + YM::OPN::LfoPmTable[FNum >> 5][m_OPN.LFO.Step >> 2][Chan.PMS]) & 0xFFF;

	/* Block shift (17-bit result) */
	uint32_t Inc = (FNum << Slot.Block) >> 2;

	/* Detune (17-bit result, might overflow) */
	Inc = (Inc + YM::OPN::Detune[Slot.KeyCode][Slot.Detune]) & 0x1FFFF;

	/* Multiply (20-bit result) */
	Inc = (Inc * Slot.Multi) >> 1;

	/* Update phase counter (20-bit) */
	Slot.PgPhase = (Slot.PgPhase + Inc) & 0xFFFFF;
}

void YM2608::UpdateEnvelopeGenerator(uint32_t SlotId)
{
	auto& Chan = m_OPN.Channel[SlotId >> 2];
	auto& Slot = m_OPN.Slot[SlotId];

	/*-------------------------------------*/
	/* Step 0: Key On / Off event handling */
	/*-------------------------------------*/
	ProcessKeyEvent(SlotId);

	/*-----------------------------*/
	/* Step 1: SSG-EG update cycle */
	/*-----------------------------*/
	if ((Slot.EgLevel >> 9) & Slot.SsgEnable)
	{
		if (Slot.KeyOn) /* Attack, decay or sustain phase */
		{
			if (Slot.SsgEgHld) /* Hold mode */
			{
				/* Set output inversion to the hold state */
				Slot.SsgEgInvOut = Slot.SsgEgInv ^ Slot.SsgEgAlt;
			}
			else /* Repeating mode */
			{
				StartEnvelope(SlotId);

				/* Flip output inversion flag (if alternating) */
				Slot.SsgEgInvOut ^= Slot.SsgEgAlt;

				/* Restart the phase counter when we are repeating normally (not alternating) */
				//if (Slot.SsgEgAlt == 0) Slot.PgPhase = 0;
				Slot.PgPhase &= ~(Slot.SsgEgAlt - 1);
			}
		}
		else /* Release phase */
		{
			/* Force the EG attenuation to maximum when we hit 0x200 during release */
			Slot.EgLevel = 0x3FF;
		}
	}

	/*-------------------------------*/
	/* Step 2: Envelope update cycle */
	/*-------------------------------*/
	if (m_OPN.EgClock == 2)
	{
		/* When attacking, move to the decay phase when attenuation level is minimal */
		if ((Slot.EgPhase | Slot.EgLevel) == 0)
		{
			Slot.EgPhase = ADSR::Decay;
		}

		/* If we reached the sustain level, move to the sustain phase */
		if ((Slot.EgPhase == ADSR::Decay) && (Slot.EgLevel >= Slot.SustainLvl))
		{
			Slot.EgPhase = ADSR::Sustain;
		}

		/* Get key scaled rate */
		uint32_t Rate = CalculateRate(Slot.EgRate[Slot.EgPhase], Slot.KeyCode, Slot.KeyScale);

		/* Get EG counter resolution */
		uint32_t Shift = YM::OPN::EgShift[Rate];
		uint32_t Mask = (1 << Shift) - 1;

		if ((m_OPN.EgCounter & Mask) == 0) /* Counter overflowed */
		{
			uint16_t Level = Slot.EgLevel;

			/* Get update cycle (8 cycles in total) */
			uint32_t Cycle = (m_OPN.EgCounter >> Shift) & 0x07;

			/* Lookup attenuation adjustment */
			uint32_t AttnInc = YM::OPN::EgLevelAdjust[Rate][Cycle];

			if (Slot.EgPhase == ADSR::Attack) /* Exponential attack */
			{
				if (Rate < 62)
				{
					Level += ((~Level * AttnInc) >> 4);
				}
			}
			else /* Linear decay */
			{
				/* When SSG-EG is active, don't update once we hit 0x200 */
				if (((Level >> 9) & Slot.SsgEnable) == 0)
				{
					Level += AttnInc << (Slot.SsgEnable << 1);

					/* Limit to maximum attenuation */
					if (Level > 0x3FF) Level = 0x3FF;
				}
			}

			Slot.EgLevel = Level;
		}
	}

	/*-------------------------------------*/
	/* Step 3: Envelope output calculation */
	/*-------------------------------------*/
	uint32_t Attn = Slot.EgLevel;

	/* Apply SGG-EG output inversion */
	if (Slot.SsgEgInvOut) Attn = (0x200 - Attn) & 0x3FF;

	/* Apply total level */
	Attn += Slot.TotalLevel;

	/* Apply AM LFO */
	Attn += (YM::OPN::LfoAmTable[m_OPN.LFO.Step][Chan.AMS] & Slot.AmOn);

	/* Limit (10-bit = 4.6 fixed point) */
	if (Attn > 0x3FF) Attn = 0x3FF;

	/* Convert from 4.6 to 4.8 fixed point */
	Slot.EgOutput = Attn << 2;
}

void YM2608::UpdateOperatorUnit(uint32_t SlotId)
{
	auto& Slot = m_OPN.Slot[SlotId];

	/* Phase modulation (10-bit) */
	uint32_t Phase = (Slot.PgPhase >> 10) + GetModulation(SlotId);

	/* Attenuation (4.8 + 4.8 = 5.8 fixed point) */
	uint32_t Level = YM::SineTable[Phase & 0x1FF] + Slot.EgOutput;

	/* dB to linear conversion (13-bit) */
	int16_t Output = YM::ExpTable[Level & 0xFF] >> (Level >> 8);

	/* Negate output (14-bit) */
	if (Phase & 0x200) Output = -Output;

	/* The last 2 generated samples are stored */
	Slot.Output[1] = Slot.Output[0];
	Slot.Output[0] = Output;
}

void YM2608::UpdateAccumulator(uint32_t ChannelId)
{
	int16_t Output = 0;
	uint32_t SlotId = ChannelId << 2;

	/* Accumulate output */
	switch (m_OPN.Channel[ChannelId].Algo)
	{
	case 0:
	case 1:
	case 2:
	case 3: /* S4 */
		Output = m_OPN.Slot[SlotId + S4].Output[0];
		break;

	case 4: /* S2 + S4 */
		Output = m_OPN.Slot[SlotId + S2].Output[0] + m_OPN.Slot[SlotId + S4].Output[0];
		break;

	case 5:
	case 6: /* S2 + S3 + S4 */
		Output = m_OPN.Slot[SlotId + S2].Output[0] + m_OPN.Slot[SlotId + S3].Output[0] + m_OPN.Slot[SlotId + S4].Output[0];
		break;

	case 7: /* S1 + S2 + S3 + S4 */
		Output = m_OPN.Slot[SlotId + S1].Output[0] + m_OPN.Slot[SlotId + S2].Output[0] + m_OPN.Slot[SlotId + S3].Output[0] + m_OPN.Slot[SlotId + S4].Output[0];
		break;
	}

	/* Limiter (signed 14-bit) */
	m_OPN.Channel[ChannelId].Output = std::clamp<int16_t>(Output, -8192, 8191);
}

int16_t YM2608::GetModulation(uint32_t Cycle)
{
	auto& Chan = m_OPN.Channel[Cycle >> 2];

	uint32_t SlotId = Cycle & 0x03;
	uint32_t ChanId = Cycle & ~0x03;

	switch (((Chan.Algo << 2) | SlotId) & 0x1F)
	{
	case 0x00: /* Algo: 0 - S1 */
	case 0x04: /* Algo: 1 - S1 */
	case 0x08: /* Algo: 2 - S1 */
	case 0x0C: /* Algo: 3 - S1 */
	case 0x10: /* Algo: 4 - S1 */
	case 0x14: /* Algo: 5 - S1 */
	case 0x18: /* Algo: 6 - S1 */
	case 0x1C: /* Algo: 7 - S1 */
		if (Chan.FB) /* Slot 1 self-feedback modulation (10-bit) */
			return (m_OPN.Slot[Cycle].Output[0] + m_OPN.Slot[Cycle].Output[1]) >> (10 - Chan.FB);
		else
			return 0;

	case 0x01: /* Algo: 0 - S2 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x02: /* Algo: 0 - S3 */
		return m_OPN.Slot[ChanId + S2].Output[0] >> 1;

	case 0x03: /* Algo: 0 - S4 */
		return m_OPN.Slot[ChanId + S3].Output[0] >> 1;

	case 0x05: /* Algo: 1 - S2 */
		return 0;

	case 0x06: /* Algo: 1 - S3 */
		return (m_OPN.Slot[ChanId + S1].Output[1] + m_OPN.Slot[ChanId + S2].Output[0]) >> 1;

	case 0x07: /* Algo: 1 - S4 */
		return m_OPN.Slot[ChanId + S3].Output[0] >> 1;

	case 0x09: /* Algo: 2 - S2 */
		return 0;

	case 0x0A: /* Algo: 2 - S3 */
		return m_OPN.Slot[ChanId + S2].Output[0] >> 1;

	case 0x0B: /* Algo: 2 - S4 */
		return (m_OPN.Slot[ChanId + S1].Output[0] + m_OPN.Slot[ChanId + S3].Output[0]) >> 1;

	case 0x0D: /* Algo: 3 - S2 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x0E: /* Algo: 3 - S3 */
		return 0;

	case 0x0F: /* Algo: 3 - S4 */
		return (m_OPN.Slot[ChanId + S2].Output[1] + m_OPN.Slot[ChanId + S3].Output[0]) >> 1;

	case 0x11: /* Algo: 4 - S2 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x12: /* Algo: 4 - S3 */
		return 0;

	case 0x13: /* Algo: 4 - S4 */
		return m_OPN.Slot[ChanId + S3].Output[0] >> 1;

	case 0x15: /* Algo: 5 - S2 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x16: /* Algo: 5 - S3 */
		return m_OPN.Slot[ChanId + S1].Output[1] >> 1;

	case 0x17: /* Algo: 5 - S4 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x19: /* Algo: 6 - S2 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x1A: /* Algo: 6 - S3 */
	case 0x1B: /* Algo: 6 - S4 */
		return 0;

	case 0x1D: /* Algo: 7 - S2 */
	case 0x1E: /* Algo: 7 - S3 */
	case 0x1F: /* Algo: 7 - S4 */
		return 0;
	}

	return 0;
}

uint8_t YM2608::CalculateRate(uint8_t Rate, uint8_t KeyCode, uint8_t KeyScale)
{
	uint8_t ScaledRate = 0;

	/* YM2608 manual page 30 */
	if (Rate != 0)
	{
		/* Calculate key scale value */
		uint8_t KSV = KeyCode >> (3 - KeyScale);

		/* Calculate key scaled rate */
		ScaledRate = (Rate << 1) + KSV;

		/* Limit to a max. of 63 */
		if (ScaledRate > 63) ScaledRate = 63;
	}

	return ScaledRate;
}

void YM2608::ProcessKeyEvent(uint32_t SlotId)
{
	auto& Slot = m_OPN.Slot[SlotId];

	/* Get latched key on/off state */
	uint32_t NewState = (Slot.KeyLatch | Slot.CsmKeyLatch);

	/* Clear CSM key on flag */
	Slot.CsmKeyLatch = 0;

	if (Slot.KeyOn ^ NewState)
	{
		if (NewState) /* Key On */
		{
			/* Start envelope */
			StartEnvelope(SlotId);

			/* Reset phase counter */
			Slot.PgPhase = 0;

			/* Set SSG-EG inverted ouput flag to the initial state when we are in any SSG-EG inverted mode */
			Slot.SsgEgInvOut = Slot.SsgEnable & Slot.SsgEgInv;
		}
		else /* Key Off */
		{
			/* Move envelope to release phase */
			Slot.EgPhase = ADSR::Release;

			if (Slot.SsgEgInvOut)
			{
				/* Allow the release phase to continue normally */
				Slot.EgLevel = (0x200 - Slot.EgLevel) & 0x3FF;

				/* Clear the SSG-EG inverted output flag */
				Slot.SsgEgInvOut = 0;
			}
		}

		Slot.KeyOn = NewState;
	}
}

void YM2608::StartEnvelope(uint32_t SlotId)
{
	auto& Slot = m_OPN.Slot[SlotId];

	/* Move envelope to attack phase */
	Slot.EgPhase = ADSR::Attack;

	/* Instant attack */
	if (CalculateRate(Slot.EgRate[ADSR::Attack], Slot.KeyCode, Slot.KeyScale) >= 62)
	{
		/* Instant minimum attenuation */
		Slot.EgLevel = 0;
	}
}

void YM2608::UpdateLFO()
{
	if (++m_OPN.LFO.Counter >= m_OPN.LFO.Period)
	{
		m_OPN.LFO.Counter = 0;
		m_OPN.LFO.Step = (m_OPN.LFO.Step + 1) & 0x7F;
	}

	m_OPN.LFO.Step &= m_OPN.LFO.Enable;
}

void YM2608::UpdateTimers()
{
	if (m_OPN.TimerA.Load)
	{
		if (--m_OPN.TimerA.Counter == 0)
		{
			m_OPN.TimerA.Counter = 1024 - m_OPN.TimerA.Period;

			/* Overflow flag enabled */
			if (m_OPN.TimerA.Enable) SetStatusFlags(FLAG_TIMERA);

			/* CSM Key On */
			if (m_OPN.ModeCSM)
			{
				/* CSM Key-On all channel 3 slots */
				m_OPN.Slot[8 + S1].CsmKeyLatch = 1;
				m_OPN.Slot[8 + S2].CsmKeyLatch = 1;
				m_OPN.Slot[8 + S3].CsmKeyLatch = 1;
				m_OPN.Slot[8 + S4].CsmKeyLatch = 1;
			}
		}
	}

	if (m_OPN.TimerB.Load)
	{
		if (--m_OPN.TimerB.Counter == 0)
		{
			m_OPN.TimerB.Counter = (256 - m_OPN.TimerB.Period) << 4;

			/* Overflow flag enabled */
			if (m_OPN.TimerB.Enable) SetStatusFlags(FLAG_TIMERB);
		}
	}
}