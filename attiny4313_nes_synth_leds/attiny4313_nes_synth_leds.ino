#include "waveforms.h"
#define CHANCOUNT 8
volatile int8_t *wav_pointer[CHANCOUNT];
uint16_t counters[CHANCOUNT];
uint16_t adders[CHANCOUNT];
uint8_t midicount, expected_midicount, LED_State_Mask;
int8_t  noisechan;
uint8_t mididata[3];
const uint8_t Leds[CHANCOUNT] = {PD2, PD3, PD4, PD5, PD6, PB0, PB1, PB2};
uint8_t channel_instrument[16];
uint8_t midinotes[CHANCOUNT], midichan[CHANCOUNT];
uint16_t noise_counter = 300;
const uint16_t duration = 300;
const PROGMEM uint16_t midi_to_freq[128] = {
  8, 8, 9, 9, 10, 10, 11, 12, 12, 13, 14, 15, 16, 17, 18, 19,
  20, 21, 23, 24, 25, 27, 29, 30, 32, 34, 36, 38, 41, 43, 46, 48,
  51, 55, 58, 61, 65, 69, 73, 77, 82, 87, 92, 97, 103, 110, 116, 123,
  130, 138, 146, 155, 164, 174, 184, 195, 207, 220, 233, 246, 261, 277, 293, 311,
  329, 349, 369, 391, 415, 440, 466, 493, 523, 554, 587, 622, 659, 698, 739, 783,
  830, 880, 932, 987, 1046, 1108, 1174, 1244, 1318, 1396, 1479, 1567, 1661, 1760, 1864, 1975,
  2093, 2217, 2349, 2489, 2637, 2793, 2959, 3135, 3322, 3520, 3729, 3951, 4186, 4434, 4698, 4978,
  5274, 5587, 5919, 6271, 6644, 7040, 7458, 7902, 8372, 8869, 9397, 9956, 10548, 11175, 11839, 12543
};
uint32_t seed = 0xFAFFAF;
int8_t get_noise()
{
  uint32_t r = seed & 0x00000001;
  seed >>= 1;
  r ^= seed & 0x00000001;
  seed |= r << 31;
  return (r << 4) - 8;
}
void change_waveform(uint8_t chan, uint8_t wave) {
  switch (wave) {
    case 0:
      {
        cli();
        wav_pointer[chan] = squarewave;
        sei();
        break;
      }
    case 1:
      {
        cli();
        wav_pointer[chan] = sine;
        sei();
        break;
      }
    case 2:
      {
        cli();
        wav_pointer[chan] = triangle;
        sei();
        break;
      }
    case 3:
      {
        cli();
        wav_pointer[chan] = saw;
        sei();
        break;
      }
    case 4:
      {
        cli();
        wav_pointer[chan] = sine_square;
        sei();
        break;
      }
    case 5:
      {
        cli();
        wav_pointer[chan] = sine2;
        sei();
        break;
      }
  }
}
void LED_Handler(uint8_t LED, uint8_t state) {
  if (state)LED_State_Mask |= (1 << LED);
  else LED_State_Mask &= ~(1 << LED);
  PORTD = (LED_State_Mask & 0x1F) << Leds[0];
  PORTB = (LED_State_Mask >> 5) | (PORTB & 0xF8);
}
int main() {
  for (uint8_t a = 0; a != CHANCOUNT; a++) {
    wav_pointer[a] = squarewave;
    midinotes[a] = 255;
    midichan[a] = 16;
  }
  for (uint8_t a = 0; a != 5; a++)DDRD |= (1 << Leds[a]);
  for (uint8_t a = 5; a != CHANCOUNT; a++)DDRB |= (1 << Leds[a]);
  DDRB |= (1 << PB3) | (1 << PB4);
  TCCR1A = 0b10100001;
  TCCR1B = 0b00000001;
  TIMSK = (1 << TOIE1);
  TCCR0A = 0x02;
  TCCR0B = 0x03;
  OCR0A = 32;
  TIMSK |= (1 << OCIE0A);
  UCSRB = 0b00011000;
  UCSRC = 0b00000110;
  UBRRH = 0;
  UBRRL = 63;
  UCSRA = (1 << U2X);
  PORTD |= _BV(PD0);
  while (1) {
    if (  UCSRA & (1 << RXC)  ) {
      uint8_t midibyte = UDR;
      if (midicount == 0) {
        uint8_t opcode = midibyte & 0xf0;
        if (midibyte < 0x80)return;
        expected_midicount = 3;
        if ((opcode == 0xC0) || (opcode == 0xD0))expected_midicount = 2;
      }
      mididata[midicount] = midibyte;
      midicount++;
      if (midicount == expected_midicount) {
        midicount = 0;
        Process_midi(mididata[0] & 0x0f);
      }
    }
  }
}
ISR(TIMER1_OVF_vect) {
  uint8_t audio_data = 128;
  for (int8_t a = 0; a != CHANCOUNT; a++) {
    counters[a] += adders[a];
    audio_data += pgm_read_byte_near(wav_pointer[a] + (counters[a] >> 10));
  }
  OCR1A = audio_data + noisechan;
}
ISR(TIMER0_COMPA_vect) {
  if (noise_counter != duration) {
    noisechan = get_noise();
    noise_counter++;
    OCR1B = 255 - (noise_counter & 0xFF);
  }
  else {
    OCR1B = 0;
    noisechan = 0;
  }
}
void Note_Handler(uint8_t midi_chan) {
  if (midi_chan == 0x09) {
    noise_counter = 0;
    return;
  }
  for (uint8_t a = 0; a != CHANCOUNT; a++) {
    if (((midi_chan == midichan[a]) && (midinotes[a] == mididata[1])) || (midinotes[a] == 255)) {
      midinotes[a] = mididata[1];
      midichan[a] = midi_chan;
      LED_Handler(a, 1);
      change_waveform(a, channel_instrument[midi_chan] % 6);
      adders[a] = 2.097152 * pgm_read_word(&(midi_to_freq[mididata[1]]));
      break;
    }
  }
}
void Process_midi(uint8_t midi_chan) {
  switch (mididata[0] & 0xf0) {
    case 0x90:
      {
        if (mididata[2]) Note_Handler(midi_chan);
        else if (midi_chan != 0x09) {
          for (uint8_t a = 0; a != CHANCOUNT; a++) {
            if ((midi_chan == midichan[a]) && (midinotes[a] == mididata[1])) {
              adders[a] = 0;
              midinotes[a] = 255;
              midichan[a] = 16;
              counters[a] = 0;
              LED_Handler(a, 0);
            }
          }
        }
        break;
      }
    case 0x80:
      {
        if (midi_chan != 0x09) {
          for (uint8_t a = 0; a != CHANCOUNT; a++) {
            if ((midi_chan == midichan[a]) && (midinotes[a] == mididata[1])) {
              adders[a] = 0;
              midinotes[a] = 255;
              midichan[a] = 16;
              counters[a] = 0;
              LED_Handler(a, 0);
            }
          }
        }
        break;
      }
    case 0xC0:
      {
        channel_instrument[midi_chan] = mididata[1];
        break;
      }
    case 0xE0:
      {
        if (midi_chan == 0x09)break;
        for (uint8_t a = 0; a != CHANCOUNT; a++) {
          if (midi_chan == midichan[a]) {
            if (midinotes[a] != 255) {
              if (mididata[2] == 0x40) {
                adders[a] = 2.097152 * pgm_read_word(&(midi_to_freq[midinotes[a]]));
              }
              else {
                volatile uint16_t difference = pgm_read_word(&(midi_to_freq[midinotes[a] + 2])) - pgm_read_word(&(midi_to_freq[midinotes[a] - 2]));
                adders[a] = 2.097152 * (((difference * mididata[2]) >> 7) + pgm_read_word(&(midi_to_freq[midinotes[a] - 2])));
              }
            }
          }
        }
        break;
      }
    case 0xB0:
      {
        if (mididata[1] == 0x78) {
          cli();
          for (uint8_t a = 0; a != CHANCOUNT; a++) {
            adders[a] = 0;
            midinotes[a] = 255;
            midichan[a] = 16;
            counters[a] = 0;
            LED_Handler(a, 0);
            wav_pointer[a] = squarewave;
          }
          sei();
        }
        break;
      }
  }
}
