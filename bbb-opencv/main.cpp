#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <vector>

#include "SPI.h"
#include "Lepton_I2C.h"

#define PACKET_SIZE 164
#define PACKET_SIZE_UINT16 (PACKET_SIZE/2)
#define PACKETS_PER_FRAME 60
#define FRAME_SIZE_UINT16 (PACKET_SIZE_UINT16*PACKETS_PER_FRAME)
#define SCALING 1

uint8_t result[PACKET_SIZE*PACKETS_PER_FRAME];

void process(cv::Mat& image) {
  //cv::threshold(image, image, 128, 255, cv::ADAPTIVE_THRESH_MEAN_C);
  cv::Canny(image, image, 60, 120, 3);

  std::vector<std::vector<cv::Point> > contours;
  std::vector<cv::Vec4i> hierarchy;
  cv::findContours(image, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_TC89_KCOS, cv::Point(0, 0));

  image.setTo(cv::Scalar(0));
  for(int i = 0; i < contours.size(); i++) {
    //if (contourArea(contours[i]) >= 25) {
      drawContours(image, contours, i, cv::Scalar(255), 1, 8, hierarchy, 2, cv::Point());
    //}
  }

  cv::Mat output;
  resize(image, output, cv::Size(640, 480));

  cv::imshow("Lepton OpenCV", output);
  cv::waitKey(1);
}

int main(int argc, char **argv)
{
	cv::Mat image(60, 80, CV_8UC1);
	cv::namedWindow("Lepton OpenCV", cv::WINDOW_AUTOSIZE);

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

		if ((frameCount++ % 3) != 0) {
			continue;
		}

		uint16_t *frameBuffer = (uint16_t *)result;
		int row, column;

#if SCALING
		uint16_t value;
		uint16_t minValue = 65535;
		uint16_t maxValue = 0;
#endif
		
		for(int i=0;i<FRAME_SIZE_UINT16;i++) {
			//skip the first 2 uint16_t's of every packet, they're 4 header bytes
			if(i % PACKET_SIZE_UINT16 < 2) {
				continue;
			}
			
			//flip the MSB and LSB at the last second
			int temp = result[i*2];
			result[i*2] = result[i*2+1];
			result[i*2+1] = temp;
			
#if SCALING
			value = frameBuffer[i];
			if(value > maxValue) {
				maxValue = value;
			}
			if(value < minValue) {
				minValue = value;
			}
			column = i % PACKET_SIZE_UINT16 - 2;
			row = i / PACKET_SIZE_UINT16 ;
#endif
		}

#if SCALING
		float diff = maxValue - minValue;
		float scale = 255/diff;
#endif

		for(int i=0;i<FRAME_SIZE_UINT16;i++) {
			if(i % PACKET_SIZE_UINT16 < 2) {
				continue;
			}
			column = (i % PACKET_SIZE_UINT16 ) - 2;
			row = i / PACKET_SIZE_UINT16;
#if SCALING
			value = (frameBuffer[i] - minValue) * scale;
			image.at<unsigned char>(row, column) = value;
#else
			image.at<unsigned char>(row, column) = (unsigned char)frameBuffer[i];
#endif
		}

		process(image);
	}
	
	SpiClosePort(0);
}
