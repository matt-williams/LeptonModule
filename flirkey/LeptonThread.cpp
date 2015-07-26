#include "LeptonThread.h"
#include <QPushButton>

#include "Palettes.h"
#include "SPI.h"
#include "Lepton_I2C.h"

#define PACKET_SIZE 164
#define PACKET_SIZE_UINT16 (PACKET_SIZE/2)
#define PACKETS_PER_FRAME 60
#define FRAME_SIZE_UINT16 (PACKET_SIZE_UINT16*PACKETS_PER_FRAME)
#define FPS 27;

LeptonThread::LeptonThread() : QThread()
{
}

LeptonThread::~LeptonThread() {
}

static unsigned int letter_counts[256] = {0, };
void LeptonThread::save(char letter) {
  char filename[20];
  sprintf(filename, "%c.%.5u.ppm", letter, letter_counts[letter]++);
  QFile file(filename);
  file.open(QIODevice::WriteOnly);
  QTextStream out(&file);
  out << "P3 80 60 65535\n";
  uint16_t* frameBuffer = (uint16_t *)result;
  for(int i=0;i<FRAME_SIZE_UINT16;i++) {
    if(i % PACKET_SIZE_UINT16 < 2) {
      continue;
    }
    out << frameBuffer[i] << " " << frameBuffer[i] << " " << frameBuffer[i] << " ";
    if (i % PACKET_SIZE_UINT16 == PACKET_SIZE_UINT16 - 1) {
      out << "\n";
    }
  }
  file.close(); 
}

void LeptonThread::triggerSave() {
  _letter = ((QPushButton*)QObject::sender())->text().at(0).toAscii();
}

void LeptonThread::run()
{
	printf(" "); // To get past initial telnet blocking

	//create the initial image
	myImage = QImage(80, 60, QImage::Format_RGB888);

	//open spi port
	SpiOpenPort(0);

	unsigned int frameCount = 0;
	while(true) {

		//read data packets from lepton over SPI
		int resets = 0;
		for(int j=0;j<PACKETS_PER_FRAME;j++) {
			//if it's a drop packet, reset j to 0, set to -1 so he'll be at 0 again loop
			read(spi_cs0_fd, result+sizeof(uint8_t)*PACKET_SIZE*j, sizeof(uint8_t)*PACKET_SIZE);
			int packetNumber = result[j*PACKET_SIZE+1];
			if(packetNumber != j) {
				j = -1;
				resets += 1;
				usleep(1000);
				//Note: we've selected 750 resets as an arbitrary limit, since there should never be 750 "null" packets between two valid transmissions at the current poll rate
				//By polling faster, developers may easily exceed this count, and the down period between frames may then be flagged as a loss of sync
				if(resets == 750) {
					SpiClosePort(0);
					usleep(750000);
					SpiOpenPort(0);
				}
			}
		}
		if(resets >= 30) {
			//qDebug() << "done reading, resets: " << resets;
		}
		if ((frameCount++ % 3) != 0) {
			continue;
		}

		frameBuffer = (uint16_t *)result;
		int row, column;
		uint16_t value;
		uint16_t minValue = 65535;
		uint16_t maxValue = 0;
		unsigned long long total = 0;

		
		for(int i=0;i<FRAME_SIZE_UINT16;i++) {
			//skip the first 2 uint16_t's of every packet, they're 4 header bytes
			if(i % PACKET_SIZE_UINT16 < 2) {
				continue;
			}
			
			//flip the MSB and LSB at the last second
			int temp = result[i*2];
			result[i*2] = result[i*2+1];
			result[i*2+1] = temp;
			
			value = frameBuffer[i];
			if(value > maxValue) {
				maxValue = value;
			}
			if(value < minValue) {
				minValue = value;
			}
			total += value;
		}
		uint16_t mean = total / 80 / 60;
		if (maxValue - mean > mean - minValue) {
			minValue = mean - (maxValue - mean);
		} else {
			maxValue = mean + (mean - minValue);
		}
		if (maxValue - minValue < 200) {
			uint16_t midValue = (maxValue + minValue) / 2;
			maxValue = midValue + 100;
			minValue = midValue - 100;
		}

                if (_letter != '\0') {
			save(_letter);
			_letter = '\0';
		}
		
		float diff = maxValue - minValue;
		float scale = 65535/diff;
		for(int i=0;i<FRAME_SIZE_UINT16;i++) {
			if(i % PACKET_SIZE_UINT16 < 2) {
				continue;
			}
			value = (frameBuffer[i] - minValue) * scale;
			column = (i % PACKET_SIZE_UINT16 ) - 2;
			row = i / PACKET_SIZE_UINT16;
			buffer[column + row * 80] = value - 32768;
		}

                process();

		for (int y = 0; y < 60; y++) {
			for (int x = 0; x < 80; x++) {
				value = buffer[x + y * 80];
				QRgb color = qRgb((value - 32768) / 256, (value - 32768) / 256, (value - 32768) / 256);
				myImage.setPixel(x, y, color);
			}
		}

		//lets emit the signal for update
		emit updateImage(myImage);

	}
	
	//finally, close SPI port just bcuz
	SpiClosePort(0);
}

extern short data_base[];
extern short data__[];
extern short data_a[];
extern short data_b[];
extern short data_c[];
extern short data_d[];
extern short data_e[];
extern short data_f[];
extern short data_i[];
extern short data_o[];
extern short data_u[];

struct prediction {
  long long accumulated;
  char letter;
  short* data;
};
static int compare_predictions(const void * a, const void * b)
{
  struct prediction* a1 = (struct prediction*)a;
  struct prediction* b1 = (struct prediction*)b;
  return (a1->accumulated > b1->accumulated) ? -1 : 
         (a1->accumulated < b1->accumulated) ? 1 : 0;
}
#define NUM_PREDICTIONS 6

static char last_prediction = '_';
static int num_prediction_ticks = 0;
void LeptonThread::process() {
	struct prediction predictions[NUM_PREDICTIONS];
	memset(predictions, 0, sizeof(predictions));
	{
		int i = 0;
		predictions[i].letter = '_'; predictions[i++].data = data__;
		predictions[i].letter = 'a'; predictions[i++].data = data_a;
		predictions[i].letter = 'e'; predictions[i++].data = data_e;
		predictions[i].letter = 'i'; predictions[i++].data = data_i;
		predictions[i].letter = 'o'; predictions[i++].data = data_o;
		predictions[i].letter = 'u'; predictions[i++].data = data_u;
	}
	for (int y = 0; y < 60; y++) {
		for (int x = 0; x < 80; x++) {
			short v = buffer[x + y * 80] / 2 - data_base[x + y * 80] / 2;
			for (int i = 0; i < NUM_PREDICTIONS; i++) {
				predictions[i].accumulated += (((long long)v) - predictions[i].data[x + y * 80]) * (((long long)v) - predictions[i].data[x + y * 80]);
				//predictions[i].accumulated += ((long long)v) * predictions[i].data[x + y * 80];
			}
			buffer[x + y * 80] = v;
		}
	}
	struct prediction sorted_predictions[NUM_PREDICTIONS];
	memcpy(sorted_predictions, predictions, sizeof(predictions));
	qsort(sorted_predictions, NUM_PREDICTIONS, sizeof(struct prediction), compare_predictions);
	
	for (int i = 0; i < NUM_PREDICTIONS; i ++) {
		if (sorted_predictions[i].accumulated > 0) {
/*
			float confidence = ((float)sorted_predictions[i].accumulated) / sorted_predictions[0].accumulated / (i + 1);
			if ((i < NUM_PREDICTIONS - 1) && (sorted_predictions[i + 1].accumulated > 0)) {
				confidence -= ((float)sorted_predictions[i + 1].accumulated) / sorted_predictions[0].accumulated / (i + 2);
			}
*/
			float confidence = ((float)sorted_predictions[i].accumulated) / sorted_predictions[0].accumulated;
			printf("%c %2.1f%% ", sorted_predictions[i].letter, confidence * 100);
			if (i == 0) {
				if (sorted_predictions[0].letter == last_prediction)
				{
//					printf("%c %d %f\n", last_prediction, num_prediction_ticks, ((float)sorted_predictions[1].accumulated) / sorted_predictions[0].accumulated);
					if (((float)sorted_predictions[1].accumulated) / sorted_predictions[0].accumulated < 0.98) {
						num_prediction_ticks++;
						if ((num_prediction_ticks == 3) && (sorted_predictions[i].letter != '_')) {
							printf("%c", sorted_predictions[i].letter);
						}
					}
				}
				else
				{
					last_prediction = sorted_predictions[0].letter;
					num_prediction_ticks = 0;
				}
			}
		}
	}
	printf("\n");
/*
	qDebug() << ((confidence > 15) ? guess : '?') << " (" << guess << " - " << confidence << ")% - " << a[0] << " " << a[1] << " " << a[2] << " " << a[3] << " " << a[4] << " " << a[5];
	if (confidence > 15) {
		printf("%c", guess);
		fflush(stdout);
	}
*/

}

void LeptonThread::performFFC() {
	//perform FFC
	lepton_perform_ffc();
}
