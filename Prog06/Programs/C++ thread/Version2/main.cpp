//
//  main.c
// CSC412 - Fall 2023 - Prog 06
//
//  Created by Jean-Yves Hervé on 2017-05-01, modified 2023-11-12
//

#include <iostream>
#include <string>
#include <random>
#include <thread>
#include <cstring>
#include <mutex>
//
#include <cstdio>
#include <cstdlib>
#include <time.h>
//
#include "gl_frontEnd.h"
#include "ImageIO_TGA.h"

using namespace std;

std::mutex myMutex;
bool continue_going = 1;
//==================================================================================
//	Function prototypes
//==================================================================================
void myKeyboard(unsigned char c, int x, int y);
void initializeApplication(std::string& outputPath, std::vector<std::string>& Vec_of_FilePaths,std::vector<RasterImage*>& imageStack);


//==================================================================================
//	Application-level global variables
//==================================================================================

//	Don't touch. These are defined in the front-end source code
extern int	gMainWindow;


//	Don't rename any of these variables/constants
//--------------------------------------------------

unsigned int numLiveFocusingThreads = 0;	//	the number of live focusing threads

//	An array of C-string where you can store things you want displayed in the spate pane
//	that you want the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
//	I preallocate the max number of messages at the max message
//	length.  This goes against some of my own principles about
//	good programming practice, but I do that so that you can
//	change the number of messages and their content "on the fly,"
//	at any point during the execution of your program, whithout
//	having to worry about allocation and resizing.
const int MAX_NUM_MESSAGES = 8;
const int MAX_LENGTH_MESSAGE = 32;
char** message;
int numMessages;
time_t launchTime;

//	This is the image that you should be writing into.  In this
//	handout, I simply read one of the input images into it.
//	You should not rename this variable unless you plan to mess
//	with the display code.
RasterImage* imageOut;

//	Random Generation stuff
random_device myRandDev;
//	If you get fancy and specialized, you may decide to go for a different engine,
//	for exemple
//		mt19937_64  Mersenne Twister 19937 generator (64 bit)
default_random_engine myEngine(myRandDev());
//	a distribution for generating random r/g/b values
uniform_int_distribution<unsigned char> colorChannelDist;
//	Two random distributions for row and column indices.  I will
//	only be able to initialize them after I have read the image
uniform_int_distribution<unsigned int> rowDist;
uniform_int_distribution<unsigned int> colDist;


//------------------------------------------------------------------
//	The variables defined here are for you to modify and add to
//------------------------------------------------------------------
#define IN_PATH		"./DataSets/Series02/"
#define OUT_PATH	"./Output/"

std::string outputPath;

//==================================================================================
//	These are the functions that tie the computation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

//	I can't see any reason why you may need/want to change this
//	function
void displayImage(GLfloat scaleX, GLfloat scaleY)
{
	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPixelZoom(scaleX, scaleY);

	//--------------------------------------------------------
	//	stuff to replace or remove.
	//	Here, each time we render the image I assign a random
	//	color to a few random pixels
	//--------------------------------------------------------
	
	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glDrawPixels(imageOut->width, imageOut->height,
				  GL_RGBA,
				  GL_UNSIGNED_BYTE,
				  imageOut->raster);

}


void displayState(void)
{
	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//--------------------------------------------------------
	//	stuff to replace or remove.
	//--------------------------------------------------------
	//	Here I hard-code a few messages that I want to see displayed in my state
	//	pane.  The number of live focusing threads will always get displayed
	//	(as long as you update the value stored in the.  No need to pass a message about it.
	time_t currentTime = time(NULL);
	numMessages = 3;
	sprintf(message[0], "System time: %ld", currentTime);
	sprintf(message[1], "Time since launch: %ld", currentTime-launchTime);
	sprintf(message[2], "I like Cheese");
	
	
	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//	You may have to synchronize this call if you run into
	//	problems, but really the OpenGL display is a hack for
	//	you to get a peek into what's happening.
	//---------------------------------------------------------
	drawState(numMessages, message);
}

void cleanupAndQuit(void)
{
	writeTGA(outputPath.c_str(), imageOut);

	//	Free allocated resource before leaving (not absolutely needed, but
	//	just nicer.  Also, if you crash there, you know something is wrong
	//	in your code.
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		free(message[k]);
	free(message);

	// delete images [optional]
	
	exit(0);
}

//	This callback function is called when a keyboard event occurs
//	You can change things here if you want to have keyboard input
//
void handleKeyboardEvent(unsigned char c, int x, int y)
{
	int ok = 0;
	
	switch (c)
	{
		//	'esc' to quit
		case 27:
			//	If you want to do some cleanup, here would be the time to do it.
			cleanupAndQuit();
			break;

		//	Feel free to add more keyboard input, but then please document that
		//	in the report.
		
		
		default:
			ok = 1;
			break;
	}
	if (!ok)
	{
		//	do something?
	}
}

//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function.
//------------------------------------------------------------------------

double convertToGrayscale(RasterImage* image, int row, int col) {
    if (image->type == RGBA32_RASTER) {
        unsigned char** raster2D = (unsigned char**) image->raster2D;
        unsigned char* pixel = raster2D[row] + col * 4;

        // Average of RGB values
        return (pixel[0] + pixel[1] + pixel[2]) / 3.0;
    }
    else if (image->type == GRAY_RASTER) {
        unsigned char** raster2D = (unsigned char**) image->raster2D;
        return raster2D[row][col];
    }
    return 0;
}


void copyPixel(RasterImage* srcImage, RasterImage* dstImage, int row, int col) {
    if (srcImage->type == RGBA32_RASTER && dstImage->type == RGBA32_RASTER) {
        unsigned char** srcRaster2D = (unsigned char**) srcImage->raster2D;
        unsigned char** dstRaster2D = (unsigned char**) dstImage->raster2D;

        for (int i = 0; i < 4; i++) { // Copy RGBA channels
            dstRaster2D[row][col * 4 + i] = srcRaster2D[row][col * 4 + i];
        }
    }
    else if (srcImage->type == GRAY_RASTER && dstImage->type == GRAY_RASTER) {
        unsigned char** srcRaster2D = (unsigned char**) srcImage->raster2D;
        unsigned char** dstRaster2D = (unsigned char**) dstImage->raster2D;

        dstRaster2D[row][col] = srcRaster2D[row][col];
    }
}



double calculateWindowContrast(RasterImage* image, int centerRow, int centerCol, int windowSize) {
    double minGray = 255.0, maxGray = 0.0;

    // Loop over the window centered at (centerRow, centerCol)
    for (int i = -windowSize / 2; i <= windowSize / 2; i++) {
        for (int j = -windowSize / 2; j <= windowSize / 2; j++) {
            unsigned int currentRow = centerRow + i;
            unsigned int currentCol = centerCol + j;

            // Check bounds
            if (currentRow >= 0 && currentRow < image->height && currentCol >= 0 && currentCol < image->width) {
                double gray = convertToGrayscale(image, currentRow, currentCol);
                minGray = std::min(minGray, gray);
                maxGray = std::max(maxGray, gray);
            }
        }
    }

    return maxGray - minGray; // Contrast is the range of grayscale values
}


void focusStackingThread(std::vector<RasterImage*> imageStack, RasterImage* outputImage) {
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distributionRow(0, outputImage->height - 1);
    std::uniform_int_distribution<int> distributionCol(0, outputImage->width - 1);
    int windowSize = 11; // Or 13 as per your requirement

    while (continue_going) {
        int centerRow = distributionRow(generator);  // Random row
        int centerCol = distributionCol(generator);  // Random column

        double highestContrast = -1.0;
        int bestImageIndex = -1;

        // Calculate contrast and find best image
        for (size_t imgIndex = 0; imgIndex < imageStack.size(); ++imgIndex) {
            double contrast = calculateWindowContrast(imageStack[imgIndex], centerRow, centerCol, windowSize);
            if (contrast > highestContrast) {
                highestContrast = contrast;
                bestImageIndex = imgIndex;
            }
        }

        // Lock for writing to the output image
        std::lock_guard<std::mutex> guard(myMutex);
        if (bestImageIndex != -1) {
            // Write pixels from the best image to the output image
            for (int i = -windowSize / 2; i <= windowSize / 2; ++i) {
                for (int j = -windowSize / 2; j <= windowSize / 2; ++j) {
                    unsigned int targetRow = centerRow + i;
                    unsigned int targetCol = centerCol + j;
                    if (targetRow >= 0 && targetRow < outputImage->height && targetCol >= 0 && targetCol < outputImage->width) {
                        copyPixel(imageStack[bestImageIndex], outputImage, targetRow, targetCol);
                    }
                }
            }
        }
    }
}


int main(int argc, char** argv)
{

	if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <num_threads> <output_path> <input_path>" << endl;
        return 1;
    }

    int numThreads = atoi(argv[1]);
    outputPath = argv[2];
	std::vector<std::string> Vec_of_FilePaths;
	for(int i = 3; i < argc; i++){
     Vec_of_FilePaths.push_back(argv[i]);
	}
	std::vector<RasterImage*> imageStack;
	//	Now we can do application-level initialization
	initializeApplication(outputPath,Vec_of_FilePaths,imageStack);

	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv, imageOut);

	// Create and start threads
	std::vector<std::thread> threads;
	for (int i = 0; i < numThreads; ++i) {
		threads.emplace_back(focusStackingThread, imageStack, imageOut);
	}

	// Wait for all threads to complete
	
	

	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();
	

	for (auto& thread : threads) {
		thread.join();
    }
	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}


//==================================================================================
//	This is a part that you have to edit and add to, for example to
//	load a complete stack of images and initialize the output image
//	(right now it is initialized simply by reading an image into it.
//==================================================================================






void initializeApplication(std::string& outputPath, std::vector<std::string>& Vec_of_FilePaths,std::vector<RasterImage*>& imageStack){

	//	I preallocate the max number of messages at the max message
	//	length.  This goes against some of my own principles about
	//	good programming practice, but I do that so that you can
	//	change the number of messages and their content "on the fly,"
	//	at any point during the execution of your program, whithout
	//	having to worry about allocation and resizing.
	message = (char**) malloc(MAX_NUM_MESSAGES*sizeof(char*));
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = (char*) malloc((MAX_LENGTH_MESSAGE+1)*sizeof(char));
	
	// Inside initializeApplication function

// Load the image stack
	for(const auto& filePath : Vec_of_FilePaths) {
		RasterImage* img = readTGA(filePath.c_str());
		imageStack.push_back(img);
	}

	// Initialize the output image
	// Assuming all images in the stack have the same dimensions
	if(!imageStack.empty()){
		imageOut = new RasterImage(imageStack[0]->width, imageStack[0]->height, imageStack[0]->type);
	}
	
	launchTime = time(NULL);
}


