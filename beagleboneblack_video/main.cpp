#include <QApplication>
#include <QThread>
#include <QMutex>
#include <QMessageBox>

#include <QColor>
#include <QLabel>
#include <QtDebug>
#include <QString>
#include <QPushButton>

#include "LeptonThread.h"
#include "MyLabel.h"

int main( int argc, char **argv )
{
	//create the app
	QApplication a( argc, argv );
	
	QWidget *myWidget = new QWidget;
	myWidget->setGeometry(400, 300, 340, 600);

	//create an image placeholder for myLabel
	//fill the top left corner with red, just bcuz
	QImage myImage;
	myImage = QImage(320, 240, QImage::Format_RGB888);
	QRgb red = qRgb(255,0,0);
	for(int i=0;i<80;i++) {
		for(int j=0;j<60;j++) {
			myImage.setPixel(i, j, red);
		}
	}

	//create a label, and set it's image to the placeholder
	MyLabel myLabel(myWidget);
	myLabel.setGeometry(10, 10, 320, 240);
	myLabel.setPixmap(QPixmap::fromImage(myImage));

	//create a FFC button
	char letters[] = "_abcdefghijklmnopqrstuvwxyz0123456789";
	QPushButton* buttons[sizeof(letters) - 1];
	for (int i = 0; i < sizeof(letters) - 1; i++) {
		char name[2];
		name[0] = letters[i];
		name[1] = '\0';
		buttons[i] = new QPushButton(name, myWidget);
		buttons[i]->setGeometry((i % 8) * 40, 320 + (i / 8) * 40, 30, 30);
	}

	//create a thread to gather SPI data
	//when the thread emits updateImage, the label should update its image accordingly
	LeptonThread *thread = new LeptonThread();
	QObject::connect(thread, SIGNAL(updateImage(QImage)), &myLabel, SLOT(setImage(QImage)));
	
	//connect ffc button to the thread's ffc action
	for (int i = 0; i < sizeof(letters) - 1; i++) {
		QObject::connect(buttons[i], SIGNAL(clicked()), thread, SLOT(triggerSave()));
	}
	thread->start();
	
	myWidget->show();

	return a.exec();
}

