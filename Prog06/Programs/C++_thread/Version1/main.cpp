#include <iostream>
#include <string>
#include <random>
#include <thread>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include "gl_frontEnd.h"
#include "ImageIO_TGA.h"

using namespace std;

//==================================================================================
//	Function prototypes
//==================================================================================

/**
 * @brief Handles keyboard events.
 * @param c Character representing the keyboard input.
 * @param x The x-coordinate of the mouse cursor.
 * @param y The y-coordinate of the mouse cursor.
 */
void myKeyboard(unsigned char c, int x, int y);

/**
 * @brief Initializes the application.
 * @param outputPath The path where the output image will be saved.
 * @param Vec_of_FilePaths Vector of file paths for input images.
 * @param imageStack Vector to store the loaded images.
 */
void initializeApplication(std::string& outputPath, std::vector<std::string>& Vec_of_FilePaths,std::vector<RasterImage*>& imageStack);

//==================================================================================
//	Application-level global variables
//==================================================================================

/** @brief External variable representing the main window in the front-end. */
extern int	gMainWindow;

/** @brief Count of the number of threads currently focusing on the image. */
unsigned int numLiveFocusingThreads = 0;

/** @brief Maximum number of messages to display. */
const int MAX_NUM_MESSAGES = 8;

/** @brief Maximum length of each message. */
const int MAX_LENGTH_MESSAGE = 32;

/** @brief Array of messages for display. */
char** message;

/** @brief Number of messages currently used. */
int numMessages;

/** @brief Time at application launch. */
time_t launchTime;

/** @brief Pointer to the output image. */
RasterImage* imageOut;

/** @brief Random device for generating random numbers. */
random_device myRandDev;

/** @brief Random engine based on myRandDev. */
default_random_engine myEngine(myRandDev());

/** @brief Distribution for generating random RGB values. */
uniform_int_distribution<unsigned char> colorChannelDist;

/** @brief Distribution for generating random row indices. */
uniform_int_distribution<unsigned int> rowDist;

/** @brief Distribution for generating random column indices. */
uniform_int_distribution<unsigned int> colDist;

/** @brief Path to input dataset. */
#define IN_PATH		"./DataSets/Series02/"

/** @brief Path for output. */
#define OUT_PATH	"./Output/"

/** @brief Path to the output image file. */
std::string outputPath;

/**
 * @brief Displays the processed image.
 * 
 * @param scaleX Scaling for x.
 * @param scaleY Scaling for y.
 */
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

/**
 * @brief Displays the information to the side of the GUI window
 */
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
	sprintf(message[2], "I like cheese");
	
	
	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//	You may have to synchronize this call if you run into
	//	problems, but really the OpenGL display is a hack for
	//	you to get a peek into what's happening.
	//---------------------------------------------------------
	drawState(numMessages, message);
}

/**
 * @brief Cleans up and exits the application.
 *
 * this function writes the image and cleans up any allocated memory
 */
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

/**
 * @brief Watches for keyboard control
 * 
 */
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

/**
 * @brief Function used to converts a pixel to its gray scale value
 * @param image Image to read from
 * @param row Row of the pixel to convert
 * @param col Column of the pixel to convert
 * @return Exit status.
 */
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

/**
 * @brief Function used to Write the best pixel to the Output Image
 * @param srcImage pointer to the image to copy from
 * @param dstImage Pointer to the image to write to
 * @param row Row of the pixel to write to
 * @param col Column of the pixel to write to 
 */
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


/**
 * @brief Function used to calculate the windows contrast
 * @param image Pointer to the image to consider 
 * @param row Row of the pixel to mofify 
 * @param col Columns of the pixel to mofify 
 * @return Contrast
 */
double calculateWindowContrast(RasterImage* image, int row, int col) {
    double minGray = 255.0, maxGray = 0.0;

    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++) {
            unsigned int neighborRow = row + i;
            unsigned int neighborCol = col + j;
            if (neighborRow >= 0 && neighborRow < image->height && neighborCol >= 0 && neighborCol < image->width) {
                double gray = convertToGrayscale(image, neighborRow, neighborCol);
                minGray = std::min(minGray, gray);
                maxGray = std::max(maxGray, gray);
            }
        }
    }

    return maxGray - minGray; // Contrast is the range of grayscale values
}

/**
 * @brief Function used for the work of each thread
 * @param imageStack Vector of pointers to each image
 * @param outputImage Pointer to the Output image
 * @param startRow Stores the Start Row for that process 
 * @param endRow Stores the end Row for that process 
 */
void focusStackingThread(std::vector<RasterImage*> imageStack, RasterImage* outputImage, int startRow, int endRow) {
    for (int row = startRow; row < endRow; ++row) {
        for (unsigned int col = 0; col < outputImage->width; ++col) {
            double highestContrast = -1.0;
            int bestImageIndex = -1;

            for (size_t imgIndex = 0; imgIndex < imageStack.size(); ++imgIndex) {
                double contrast = calculateWindowContrast(imageStack[imgIndex], row, col);
                if (contrast > highestContrast) {
                    highestContrast = contrast;
                    bestImageIndex = imgIndex;
                }
            }

            if (bestImageIndex != -1) {
                copyPixel(imageStack[bestImageIndex], outputImage, row, col);
            }
        }
    }
}

/**
 * @brief Main function of the application.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status.
 */
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


	int rowsPerThread = imageOut->height / numThreads;

	// Create and start threads
	std::vector<std::thread> threads;
	for (int i = 0; i < numThreads; ++i) {
		int startRow = i * rowsPerThread;
		int endRow = (i == numThreads - 1) ? imageOut->height : startRow + rowsPerThread;
		threads.emplace_back(focusStackingThread, imageStack, imageOut, startRow, endRow);
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





/**
 * @brief Initalizes the main componants of the program 
 * @param Vec_of_FilePaths Vector of each of the file paths
 * @param imageStack Stack of pointers to the images
 */
void initializeApplication(std::string& outputPath, std::vector<std::string>& Vec_of_FilePaths,std::vector<RasterImage*>& imageStack)
{

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



