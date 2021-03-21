// Intervalism
// 20210321
// Daniel Skaborn

#include "/usr/include/alsa/asoundlib.h"
#include "/usr/include/alsa/pcm.h"
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#define SAMPLERATE		48000
#define SAMPLERATEF		48000.0f
#define	CHANNEL			7

char mididevice[100];
char alsadevice[100];

pthread_t MIDIrcv;
void *cmdMIDIrcv(void *arg);

int MIDIin_d;
int MIDIout_d;
int AUDIOout_d;

snd_output_t *output = NULL;
snd_pcm_sframes_t frames;

int16_t audiobuffer1[256];
int16_t audiobuffer2[256];
volatile unsigned char activebuffer = 0;
volatile unsigned char bufferfull = 0;
snd_pcm_t *handle;

volatile float	frq[4];
volatile float	env[4];
volatile float	pitchBend;

int MIDIin(unsigned char *data) {
	return read(MIDIin_d, data, 1 );
}

void MIDIout(unsigned char outbuf) {
	write(MIDIout_d, &outbuf, 1);
	return;
}

void AudioOut(float l, float r) {
	static int samplecount = 0;
	static int buffersamples = sizeof(audiobuffer1)/2;
	snd_pcm_sframes_t frames;
	switch (activebuffer) {
		case 0x0:
			audiobuffer1[samplecount]=(int16_t)(l*0x7FFF);
			samplecount++;
			audiobuffer1[samplecount]=(int16_t)(r*0x7FFF);
			samplecount++;
			break;
		case 0xFF:
			audiobuffer2[samplecount]=(int16_t)(l*0x7FFF);
			samplecount++;
			audiobuffer2[samplecount]=(int16_t)(r*0x7FFF);
			samplecount++;
			break;
		default:
			printf("Buffer toggle error!\n");
			activebuffer=0;
			break;
	}
	if (samplecount == buffersamples) {
		samplecount=0;
		if (activebuffer==0) {
			while ( (frames = snd_pcm_writei(handle, audiobuffer1, sizeof(audiobuffer1)/4)) == EAGAIN)
				;
		}
		else {
			while ( (frames = (snd_pcm_writei(handle, audiobuffer2, sizeof(audiobuffer2)/4))) == EAGAIN)
				;
		}
		if (frames < 0) {
			snd_pcm_close(handle);
			if (snd_pcm_open(&handle, alsadevice, SND_PCM_STREAM_PLAYBACK,0)<0) {
				printf("ALSA sound write error: %s %i\n",snd_strerror(frames),(int)(frames));
				printf("Unable to reopen audiodevice\n");
				exit(0);
			}
			snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, SAMPLERATE, 0, 20000);
		}
		activebuffer = ~activebuffer;
	}
	return;
}

float wavetable[8193][8];
float wavetableFetch(float pos, unsigned char table) {
	int temppos;
	float offset;

	temppos = (int)pos;
	offset = pos - (float)temppos;
	if (temppos == 4095) { // handle wrap
		return wavetable[temppos][table] + (wavetable[0][table]-wavetable[temppos][table]) * offset;
	} else {
		return wavetable[temppos][table] + (wavetable[temppos+1][table]-wavetable[temppos][table]) * offset;
	}
}

void itvl(int osc, int a) {
	int i;
	switch(a) {
	case 1:
		frq[osc] = frq[osc] * 16.0/15.0;
		break;
	case 2:
		frq[osc] = frq[osc] * 9.0/8.0;
		break;
	case 3:
		frq[osc] = frq[osc] * 6.0/5.0;
		break;
	case 4:
		frq[osc] = frq[osc] * 5.0/4.0;
		break;
	case -1:
		frq[osc] = frq[osc] * 15.0/16.0;
		break;
	case -2:
		frq[osc] = frq[osc] * 8.0/9.0;
		break;
	case -3:
		frq[osc] = frq[osc] * 5.0/6.0;
		break;
	case -4:
		frq[osc] = frq[osc] * 4.0/5.0;
		break;
	}

	for (i=0;i<4;i++)
		env[osc] = 0.25;
	env[osc] = 0.5;
	printf("%d:%f\n",osc,frq[osc]);
	return;
}

void lc1freqdecode(unsigned char key) {
	switch (key) {
		case 0x40:
			frq[0] = 220;
			break;
		case 0x41:
			frq[1] = 220;
			break;
		case 0x42:
			frq[2] = 220;
			break;
		case 0x43:
			frq[3] = 220;
			break;
// osc 0:
		case 0x2c: // up1
			itvl(0,1);
			break;
		case 0x28: // up2
			itvl(0,2);
			break;
		case 0x24: // up3
			itvl(0,3);
			break;
		case 0x20: // up4
			itvl(0,4);
			break;
		case 0x30: // dwn1
			itvl(0,-1);
			break;
		case 0x34: // dwn2
			itvl(0,-2);
			break;
		case 0x38: // dwn3
			itvl(0,-3);
			break;
		case 0x3c: // dwn4
			itvl(0,-4);
			break;
		
// osc 1:
		case 0x2d: // up1
			itvl(1,1);
			break;
		case 0x29: // up2
			itvl(1,2);
			break;
		case 0x25: // up3
			itvl(1,3);
			break;
		case 0x21: // up4
			itvl(1,4);
			break;
		case 0x31: // dwn1
			itvl(1,-1);
			break;
		case 0x35: // dwn2
			itvl(1,-2);
			break;
		case 0x39: // dwn3
			itvl(1,-3);
			break;
		case 0x3d: // dwn4
			itvl(1,-4);
			break;
			
// osc 2:
		case 0x2e: // up1
			itvl(2,1);
			break;
		case 0x2a: // up2
			itvl(2,2);
			break;
		case 0x26: // up3
			itvl(2,3);
			break;
		case 0x22: // up4
			itvl(2,4);
			break;
		case 0x32: // dwn1
			itvl(2,-1);
			break;
		case 0x36: // dwn2
			itvl(2,-2);
			break;
		case 0x3a: // dwn3
			itvl(2,-3);
			break;
		case 0x3e: // dwn3
			itvl(2,-4);
			break;
			
// osc 3:
		case 0x2f: // up1
			itvl(3,1);
			break;
		case 0x2b: // up2
			itvl(3,2);
			break;
		case 0x27: // up3
			itvl(3,3);
			break;
		case 0x23: // up4
			itvl(3,4);
			break;
		case 0x33: // dwn1
			itvl(3,-1);
			break;
		case 0x37: // dwn2
			itvl(3,-2);
			break;
		case 0x3b: // dwn3
			itvl(3,-3);
			break;
		case 0x3f: // dwn4
			itvl(3,-4);
			break;
	}
}

void *cmdMIDIrcv(void *arg) {
	unsigned char mididata;
	int mps=0;
	char tmp1=0, tmp2=0, tmp3=0;
	unsigned char v;
	unsigned char takeV;

	int pitchBendRaw;

	while(1) {
		if( MIDIin(&mididata)==1 ) {
			
//			printf("mi: %x\n",mididata);
			switch (mps) {
				case 0: // wait for any data
					//if (inbuf == (0xB0+CHANNEL)) mps=1; // CC
					if (mididata == (0x90+CHANNEL)) mps=3; // Note on
					//if (inbuf == (0x80+CHANNEL)) mps=5; // Note off
					//if (inbuf == (0xC0+CHANNEL)) mps=7; // program change
					//if (inbuf == (0xE0+CHANNEL)) mps=8; // pitchbend
					//if (inbuf == (0xF0)) mps=10; // sysex
					break;

			case 1: // CC #
				tmp1 = mididata;
				printf("cc: %x\n",tmp1);
				mps=2;
				break;
			case 2: // CC value
/*				if (tmp1==123) { // all notes off
				
				} else {
					if ((tmp1<121) && (inbuf<0x80)) {
					* 	
					}
				}*/
				mps=0;
				break;

			case 3: // Note On
				tmp1 = mididata; // key#
				printf("no: %x\n",tmp1);
				lc1freqdecode(tmp1);
				mps=4;
				break;
				
			case 4: // Note On velocity
				//if (mididata==0) { // Note off
				//} else { // Note on
					//if (gate[0] != 0) {
						//note[1]=tmp1;
						//gate[1]=mididata;
					//} else {
						//note[0] = note[1] = tmp1;
						//gate[0] = gate[1] = mididata;
					//}
					//polyVoiceAllocate(tmp1, mididata);			
				//}
				mps=0;
				break;

			case 5: // Notes off

				mps=6;
				break;
			case 6: // Note off

				mps=0;
				break;

			case 7: // Program change
				mps=0;
				break;

			case 8: // Pitchbend
				tmp1= mididata;
				mps=9;
				break;
			case 9:
				pitchBendRaw = tmp1 + (mididata * 0x100);
				pitchBend = (float)(pitchBendRaw) / 100.0;
				mps=0;
				break;
			default:
			mps=0;
			}
		}
	}
}

void theIntervalist(void) {

	float osc[4];
	float pha[4];
	float tfrq[4];

	int i;
	
	for (i=0;i<8192;i++)
		wavetable[i][0] = sinf(2.0*3.14159265359*(float)(i)/8192.0);
	
	for (i=0;i<4;i++) {
		frq[i] = 220;
		tfrq[i] = 220;
		pha[i] = 0.0;
		env[i] = 0.0;
	}
	
	
	while(1) {

		for (i=0;i<4;i++) {
/*			if (tfrq[i]<frq[i])
				tfrq[i]+=0.01;
			if (tfrq[i]>frq[i])
				tfrq[i]-=0.05;*/
			pha[i] = pha[i] + (8192.0/SAMPLERATEF * frq[i]);
			if (pha[i] > 8192.0) pha[i] = pha[i] - 8192.0;
			env[i] *=0.99995;
			osc[i] = wavetableFetch(pha[i], 0) * env[i];
		}
		
		AudioOut((osc[0]+osc[1]), (osc[2]+osc[3]));
		
	}
	return;
}

int main(void)
{
	int flags;
	
	FILE *paramfil;

	paramfil = fopen("setting.txt","r");
	if (paramfil==NULL) { printf("System setupfile setting.txt not found!\n"); exit(0); }

	fscanf(paramfil,"%s\n%s\n",mididevice, alsadevice);
	printf("Audio and MIDI Initialization:\n");

	// Init MIDI device
	printf("\n  MIDI:  %s", mididevice);
	MIDIin_d  = open(mididevice,O_RDONLY);
	MIDIout_d = open(mididevice,O_WRONLY);
	if (MIDIin_d==-1)  { printf("\n         Could not open MIDI in %s\n\n", mididevice); exit(0); }
	if (MIDIout_d==-1) { printf("\n         Could not open MIDI out %s\n\n", mididevice); exit(0); }
	flags = fcntl(MIDIin_d, F_GETFL, 0);
	fcntl(MIDIin_d, F_SETFL, flags | O_NONBLOCK);
	printf("\n         MIDI init done\n");

	// Init ALSA sound device
	printf("\n  Audio: %s", alsadevice);
 	if ( snd_pcm_open(&handle, alsadevice, SND_PCM_STREAM_PLAYBACK, 0)<0 ) {
		printf("\n         ALSA init/open error!\n\n");
		exit(0);
	}
	snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, SAMPLERATE, 0, 20000 );

	printf("\n         ALSA init done\n");

	printf("\n         Starting the MIDI receive thread...\n");
	pthread_create(&MIDIrcv, NULL, cmdMIDIrcv, NULL);

	printf("\nStarting theIntervalist...\n");
	theIntervalist();

	snd_pcm_close(handle);
	close(MIDIin_d);
	close(MIDIout_d);
	return 0;
}
