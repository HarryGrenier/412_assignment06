/**
 * @file main.cpp
 * @brief Example of Multithreaded with a multiple locks using C++ threads
 * 
 * This is an example of focus Stacking an image
 * 
 * @author Harry Grenier
 * @date 12/3/2023
 */

#include <iostream>
#include <string>
#include <random>
#include <thread>
#include <cstring>
#include <set>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include "gl_frontEnd.h"
#include "ImageIO_TGA.h"

using namespace std;

/** @brief Global mutex used for synchronization. */
std::mutex myMutex;

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
void initializeApplication(std::vector<std::string>& Vec_of_FilePaths,std::vector<RasterImage*>& imageStack);

/**
 * @brief Function used to calculate the windows contrast
 * @param image Pointer to the image to consider 
 * @param centerRow Stores the middle row of the Window 
 * @param centerCol Stores the middle collumn of the window
 * @param windowSize Stores the size of the window to examine
 * @return Contrast
 */
double calculateWindowContrast(RasterImage* image, int centerRow, int centerCol, int windowSize);

/**
 * @brief Function used for the work of each thread
 * @param imageStack Vector of pointers to each image
 * @param outputImage Pointer to the Output image
 * @param startRow Stores the Start Row for that process 
 * @param endRow Stores the end Row for that process 
 */
void focusStackingThread(std::vector<RasterImage*> imageStack, RasterImage* outputImage,int startRow, int endRow);

/**
 * @brief Function used to Write the best pixel to the Output Image
 * @param srcImage pointer to the image to copy from
 * @param dstImage Pointer to the image to write to
 * @param row Row of the pixel to write to
 * @param col Column of the pixel to write to 
 */
void copyPixel(RasterImage* srcImage, RasterImage* dstImage, int row, int col);

/**
 * @brief Function used to converts a pixel to its gray scale value
 * @param image Image to read from
 * @param row Row of the pixel to convert
 * @param col Column of the pixel to convert
 * @return Exit status.
 */
double convertToGrayscale(RasterImage* image, int row, int col);


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

/** @brief Number of rows in the image grid. */
const int GRID_ROWS = 4;

/** @brief Number of columns in the image grid. */
const int GRID_COLS = 4;

/** @brief 2D array of mutexes for locking different regions of the image. */
std::mutex gridMutexes[GRID_ROWS][GRID_COLS];

/** @brief Mutex for synchronizing access to the output image. */
std::mutex imageMutex;

/** @brief Vector of threads used for processing. */
std::vector<std::thread> threads;

/** @brief Vector of unique pointers to mutexes for region-based locking. */
std::vector<std::unique_ptr<std::mutex>> regionMutexes;

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

	time_t currentTime = time(NULL);
	numMessages = 3;
	sprintf(message[0], "System time: %ld", currentTime);
	sprintf(message[1], "Time since launch: %ld", currentTime-launchTime);
	sprintf(message[2], "I like Cheese");
	
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

	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		free(message[k]);
	free(message);
	
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

/**
 * @brief Main function of the application.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status.
 */
int main(int argc, char** argv){

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


	initializeApplication(Vec_of_FilePaths,imageStack);

	initializeFrontEnd(argc, argv, imageOut);

    int rowsPerThread = imageOut->height / numThreads;

	for (int i = 0; i < numThreads; ++i) {
        unsigned int startRow = i * rowsPerThread;
        unsigned int endRow = (i == numThreads - 1) ? imageOut->height : startRow + rowsPerThread;
        threads.emplace_back(focusStackingThread, imageStack, imageOut, startRow, endRow);
    }

	glutMainLoop();
	

	for (auto& thread : threads) {
		thread.join();
    }

	return 0;
}

/**
 * @brief Initalizes the main componants of the program 
 * @param Vec_of_FilePaths Vector of each of the file paths
 * @param imageStack Stack of pointers to the images
 */
void initializeApplication(std::vector<std::string>& Vec_of_FilePaths,std::vector<RasterImage*>& imageStack){

	message = (char**) malloc(MAX_NUM_MESSAGES*sizeof(char*));
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = (char*) malloc((MAX_LENGTH_MESSAGE+1)*sizeof(char));

	// Load the image stack
	for(const auto& filePath : Vec_of_FilePaths) {
		RasterImage* img = readTGA(filePath.c_str());
		imageStack.push_back(img);
	}

	int numRegions = GRID_ROWS * GRID_COLS; // Calculate the total number of regions
    regionMutexes.resize(numRegions);
    for (int i = 0; i < numRegions; ++i) {
        regionMutexes[i] = std::make_unique<std::mutex>();
    }

	if(!imageStack.empty()){
		imageOut = new RasterImage(imageStack[0]->width, imageStack[0]->height, imageStack[0]->type);
	}
	
	launchTime = time(NULL);
}

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

        for (int i = 0; i < 4; i++) {
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
 * @param centerRow Stores the middle row of the Window 
 * @param centerCol Stores the middle collumn of the window
 * @param windowSize Stores the size of the window to examine
 * @return Contrast
 */
double calculateWindowContrast(RasterImage* image, int centerRow, int centerCol, int windowSize) {
    double minGray = 255.0, maxGray = 0.0;
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

/**
 * @brief Function used for the work of each thread
 * @param imageStack Vector of pointers to each image
 * @param outputImage Pointer to the Output image
 * @param startRow Stores the Start Row for that process 
 * @param endRow Stores the end Row for that process 
 */
void focusStackingThread(std::vector<RasterImage*> imageStack, RasterImage* outputImage,int startRow, int endRow) {
	int windowSize = 11;
    std::default_random_engine generator(std::random_device{}());
    std::uniform_int_distribution<int> distributionRow(startRow, endRow - 1);
    std::uniform_int_distribution<int> distributionCol(0, outputImage->width - 1);

    while (true) {
        int centerRow = distributionRow(generator);  // Random row
        int centerCol = distributionCol(generator);  // Random column


		std::set<int> uniqueRegionIndices;
        for (int i = -windowSize / 2; i <= windowSize / 2; ++i) {
            for (int j = -windowSize / 2; j <= windowSize / 2; ++j) {
                int targetRow = centerRow + i;
                unsigned int targetCol = centerCol + j;
                if (targetRow >= startRow && targetRow < endRow && targetCol >= 0 && targetCol < outputImage->width) {
                    int regionIndex = (targetRow / (outputImage->height / GRID_ROWS)) * GRID_COLS + (targetCol / (outputImage->width / GRID_COLS));
                    uniqueRegionIndices.insert(regionIndex);
                }
            }
        }

         // Create a vector of mutex pointers and sort them
        for (const auto& regionIndex : uniqueRegionIndices) {
            regionMutexes[regionIndex]->lock();
        }

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
        if (bestImageIndex != -1) {
			std::lock_guard<std::mutex> guard(imageMutex);
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

		for (const auto& regionIndex : uniqueRegionIndices) {
            regionMutexes[regionIndex]->unlock();
        }
}
}
