

#include <windows.h>		// Header File For Windows
#include <gl\gl.h>			// Header File For The OpenGL32 Library
#include <gl\glu.h>			// Header File For The GLu32 Library
#include <GLFW\glfw3.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <unordered_map>
#include <map>
#include <queue>

using namespace cv;

#define     MAP_SIZE		1024                // OpenGL Variables and Constants
#define     HEIGHT_RATIO    1.0f       

HDC			hDC = NULL;						
HGLRC		hRC = NULL;							
HWND		hWnd = NULL;						
HINSTANCE	hInstance;							

bool		keys[256];								
bool		active = TRUE;							
bool		fullscreen = TRUE;
bool		bRender = TRUE;

std::string path;								// image and matrix variables
Mat image;
unsigned char data[4];
void loadImageMat();
void loadCollisionMat();

bool    highRes = false;						//resolution differentation
int		highResStepSize = 8;					
int		highResCols;
int		highResRows;
int		lowResStepSize = 16;
int		lowResCols;
int		lowResRows;
int     collisionCols;
int     collisionRows;

enum	Operation {								//supported operations: bfs, bfs with collision detection, dfs
			Empty,
			Bfs,
			CollisionBfs,
			Dfs
};
Operation operation;
bool	  operationActive;
int		  source = -1;
int		  dest = -1;
std::unordered_map<int, int>  pickingMap;
std::unordered_map<int, bool>  collisionMap;
std::unordered_map<int, bool> visited;
int size;
bool leftClick = false;

float scaleValue = 0.15f;						// Scaling values and variables
int width;
int height;
const float piover180 = 0.0174532925f;
float heading;
float xpos;
float zpos;
GLfloat	yrot;				
GLfloat	xrot;				
GLfloat	zrot;				

GLfloat walkbias = 0;
GLfloat walkbiasangle = 0;
GLfloat lookupdown = 0.0f;
GLfloat	z = 0.0f;				

typedef struct TriPoint {						//Triangle struct decleration
	GLint x;
	GLint y;
}TriPoint;

typedef struct TriPointColor {
	GLfloat z;
	GLfloat r;
	GLfloat g;
	GLfloat b;
}TriPointColor;

typedef struct Triangle {
	TriPoint p1;
	TriPointColor c1;
	TriPoint p2;
	TriPointColor c2;
	TriPoint p3;
	TriPointColor c3;

	bool isPicked;
	GLfloat left_r;
	GLfloat left_g;
	GLfloat left_b;
	int parentId; 
	int vectorIndex;
	bool collision;
	int bfsParentId;
	int maxZ;
}Triangle;

std::vector<Triangle> vectorLowRes;					//triangle vectors that hold the loaded triangles
std::vector<Triangle> vectorHighRes;
std::vector<Triangle> collisionVector;
std::vector<Triangle>* currentTriangleVectorPtr;

LRESULT	CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);	

GLvoid ReSizeGLScene(GLsizei width, GLsizei height)
{
	if (height == 0)										
	{
		height = 1;										
	}

	glViewport(0, 0, width, height);						

	glMatrixMode(GL_PROJECTION);						
	glLoadIdentity();									

	// Calculate The Aspect Ratio Of The Window
	gluPerspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 500.0f);

	glMatrixMode(GL_MODELVIEW);							
	glLoadIdentity();
}

void LoadFile() {	
	image = imread("C:/Users/nspie/Desktop/Picture2.png", IMREAD_COLOR);
	//image = imread("C:/Users/nspie/Desktop/Picture1.png", IMREAD_COLOR);
	//std::string image_path = cv::samples::findFile("starry_night.jpg"); /*TODO: RECIEVE PATH AS INPUT*/
	//image = imread(image_path, IMREAD_COLOR);
	if (image.empty()) {
		std::cout << "Could not read the image: " <<  std::endl;
		exit(0);
	}
	width = image.cols;
	height = image.rows;
	loadImageMat();
	highResCols = lowResCols * 2;
	highResRows = lowResRows * 2;
	loadCollisionMat();
}

int InitGL(GLvoid)										// All Setup For OpenGL Goes Here
{
	glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);				// Black Background
	glClearDepth(1.0f);									// Depth Buffer Setup
	glDepthFunc(GL_LEQUAL);								// The Type Of Depth Testing To Do
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations
	LoadFile();
	return TRUE;										// Initialization Went OK
}

void setPointAttributes(TriPoint& point, TriPointColor& color, GLint x, GLint y) {
	point.x = x;
	point.y = y;
	cv::Vec3b pixel = image.at<Vec3b>(x, y);

	uchar b = image.at<Vec3b>(x, y)[0];
	uchar g = image.at<Vec3b>(x, y)[1];
	uchar r = image.at<Vec3b>(x, y)[2];
	float zTmp = (r + b + g) / 3;
	
	
	color.z =  60.0f * (zTmp / 255.0f);
	color.b = pixel[0]/255.0f;
	color.g = pixel[1]/255.0f;
	color.r = pixel[2]/255.0f;
}

void setTriangleAttibutes(Triangle& T, TriPoint& p1, TriPointColor& c1, TriPoint& p2, TriPointColor& c2, TriPoint& p3, TriPointColor& c3, int index, int ID) {
	T.p1 = p1;
	T.p2 = p2;
	T.p3 = p3;
	T.c1 = c1;
	T.c2 = c2;
	T.c3 = c3;

	T.left_r = (index & 0x000000FF) >> 0;
	T.left_g = (index & 0x0000FF00) >> 8;
	T.left_b = (index & 0x00FF0000) >> 16;
	T.isPicked = false;
	T.parentId = ID;
	T.vectorIndex = index;
	T.collision = false;
	T.maxZ = c1.z;
	T.bfsParentId = -1;
	if (c2.z > T.maxZ)
		T.maxZ = c2.z;
	if (c3.z > T.maxZ)
		T.maxZ = c3.z;

}

void TriBuilder(int x1, int y1, int x2, int y2, int x3, int y3, std::vector<Triangle>* vectorT, int ID) {
	TriPoint p1;
	TriPointColor c1;
	setPointAttributes(p1, c1, x1, y1);

	TriPoint p2;
	TriPointColor c2;
	setPointAttributes(p2, c2, x2, y2);

	TriPoint p3;
	TriPointColor c3;
	setPointAttributes(p3, c3, x3, y3);

	Triangle T1;
	setTriangleAttibutes(T1, p1, c1, p2, c2, p3, c3, vectorT->size(), ID);
	vectorT->push_back(T1);
}

void loadCollisionMat() {
	for (GLint x = 0; x % lowResStepSize != 0 || x < image.rows - lowResStepSize; x += highResStepSize) {
		for (GLint y = 0; y % lowResStepSize != 0 || y < image.cols - lowResStepSize; y += highResStepSize) {
			TriBuilder(x, y, x, y + highResStepSize, x + highResStepSize, y, &collisionVector, -1);
			TriBuilder(x, y + highResStepSize, x + highResStepSize, y, x + highResStepSize, y + highResStepSize, &collisionVector, -1);
		}
	}
}

void loadImageMat() {
	int parentId = 0;
	for (GLint x = 0; x < image.rows - lowResStepSize; x += lowResStepSize) {
		lowResRows++;
		for (GLint y = 0; y < image.cols - lowResStepSize; y += lowResStepSize) {
			if (lowResRows == 1)
				lowResCols++;
				//Load the small triangles
				TriBuilder(x, y, x, y + highResStepSize, x + highResStepSize, y, &vectorHighRes, parentId);
				TriBuilder(x, y + highResStepSize, x + highResStepSize, y, x+ highResStepSize, y+ highResStepSize, &vectorHighRes, parentId);
				TriBuilder(x, y + highResStepSize, x, y + (2 * highResStepSize), x + highResStepSize, y + highResStepSize, &vectorHighRes, parentId);
				TriBuilder(x + highResStepSize, y, x + highResStepSize, y + highResStepSize, x + (2 * highResStepSize), y, &vectorHighRes, parentId);
				parentId++;
				TriBuilder(x + highResStepSize, y + highResStepSize, x + highResStepSize, y + (2 * highResStepSize), x + (2 * highResStepSize), y + highResStepSize, &vectorHighRes, parentId);
				TriBuilder(x + highResStepSize, y + highResStepSize, x + highResStepSize, y + (2 * highResStepSize), x, y + (2 * highResStepSize), &vectorHighRes, parentId);
				TriBuilder(x + highResStepSize, y + highResStepSize, x + (2 * highResStepSize), y, x + (2 * highResStepSize), y + highResStepSize, &vectorHighRes, parentId);
				TriBuilder(x + highResStepSize, y + (2 * highResStepSize), x + (2 * highResStepSize), y + highResStepSize, x + (2 * highResStepSize), y + (2 * highResStepSize), &vectorHighRes, parentId);
				parentId++;

				//Load the large triangles
				TriBuilder(x, y, x, y + lowResStepSize, x + lowResStepSize, y, &vectorLowRes, -1);
				TriBuilder(x, y + lowResStepSize, x + lowResStepSize, y, x + lowResStepSize, y + lowResStepSize, &vectorLowRes, -1);
		}

	}
	size = vectorLowRes.size();
	currentTriangleVectorPtr = &vectorLowRes;
}

bool isFirstCol(int index) {
	return (index % (collisionCols * 2) == 0 );
}

bool isLastCol(int index) {
	return (index % (collisionCols * 2) == collisionCols*2 - 1);
}

bool isFirstRow(int index) {
	return (index / (collisionCols * 2) == 0);
}

bool isLastRow(int index) {
	return (index % (collisionCols * 2) == collisionRows - 1);
}

bool bfs(int from, int to) {
	std::queue<int> idQueue;
	idQueue.push(from);
	int curIndex;
	while (!idQueue.empty()) {
		curIndex = idQueue.front();
		idQueue.pop();
		if (curIndex == to)
			return true;
		if (curIndex % 2 == 0) {

			if ((curIndex + 1) < currentTriangleVectorPtr->size() && (*currentTriangleVectorPtr)[curIndex + 1].bfsParentId == -1) {
				(*currentTriangleVectorPtr)[curIndex + 1].bfsParentId = curIndex;
				idQueue.push(curIndex + 1);
			}

			if (!isFirstCol(curIndex)) {
				if (curIndex - 1 >= 0 && (*currentTriangleVectorPtr)[curIndex - 1].bfsParentId == -1) {
					(*currentTriangleVectorPtr)[curIndex - 1].bfsParentId = curIndex;
					idQueue.push(curIndex - 1);
				}
			}

			if (!isFirstRow(curIndex)) {
				if ((curIndex - collisionCols * 2 + 1 >= 0) && (*currentTriangleVectorPtr)[curIndex - collisionCols * 2 + 1].bfsParentId == -1) {
					(*currentTriangleVectorPtr)[curIndex - collisionCols * 2 + 1].bfsParentId = curIndex;
					idQueue.push(curIndex - collisionCols * 2 + 1);
				}
			}

		}
		else {

			if (!isLastCol(curIndex)) {
				if ((curIndex + 1) < currentTriangleVectorPtr->size() && (*currentTriangleVectorPtr)[curIndex + 1].bfsParentId == -1) {
					(*currentTriangleVectorPtr)[curIndex + 1].bfsParentId = curIndex;
					idQueue.push(curIndex + 1);
				}
			}

			if ((curIndex - 1) >= 0 && (*currentTriangleVectorPtr)[curIndex - 1].bfsParentId == -1) {
				(*currentTriangleVectorPtr)[curIndex - 1].bfsParentId = curIndex;
				idQueue.push(curIndex -1);
			}
			if (!isLastRow(curIndex)) {
				if ((curIndex + collisionCols * 2 - 1) < currentTriangleVectorPtr->size() && (*currentTriangleVectorPtr)[curIndex + collisionCols * 2 - 1].bfsParentId == -1) {
					(*currentTriangleVectorPtr)[curIndex + collisionCols * 2 - 1].bfsParentId = curIndex;
					idQueue.push(curIndex + collisionCols * 2 - 1);
				}
			}
		}
	}
}

bool bfsCollision(int from, int to, int maxHeight) {
	std::queue<int> idQueue;
	idQueue.push(from);
	int curIndex;
	while (!idQueue.empty()) {
		curIndex = idQueue.front();
		idQueue.pop();
		if (curIndex == to)
			return true;
		if (curIndex % 2 == 0) {

			if ((curIndex + 1) < currentTriangleVectorPtr->size() && (*currentTriangleVectorPtr)[curIndex + 1].bfsParentId == -1) {
				if ((*currentTriangleVectorPtr)[curIndex + 1].maxZ <= maxHeight) {
					(*currentTriangleVectorPtr)[curIndex + 1].bfsParentId = curIndex;
					idQueue.push(curIndex + 1);
				}
				else {
					(*currentTriangleVectorPtr)[curIndex + 1].bfsParentId == -2;
				}
				
			}

			if (!isFirstCol(curIndex)) {
				if (curIndex - 1 >= 0 && (*currentTriangleVectorPtr)[curIndex - 1].bfsParentId == -1) {
					if ((*currentTriangleVectorPtr)[curIndex - 1].maxZ <= maxHeight) {
						(*currentTriangleVectorPtr)[curIndex - 1].bfsParentId = curIndex;
						idQueue.push(curIndex - 1);
					}
					else {
						(*currentTriangleVectorPtr)[curIndex - 1].bfsParentId == -2;
					}
				}
			}

			if (!isFirstRow(curIndex)) {
				if ((curIndex - collisionCols * 2 + 1 >= 0) && (*currentTriangleVectorPtr)[curIndex - collisionCols * 2 + 1].bfsParentId == -1) {
					if ((*currentTriangleVectorPtr)[curIndex - collisionCols * 2 + 1].maxZ <= maxHeight) {
						(*currentTriangleVectorPtr)[curIndex - collisionCols * 2 + 1].bfsParentId = curIndex;
						idQueue.push(curIndex - collisionCols * 2 + 1);
					}
					else {
						(*currentTriangleVectorPtr)[curIndex - collisionCols * 2 + 1].bfsParentId == -2;
					}
				}
			}

		}
		else {

			if (!isLastCol(curIndex)) {
				if ((curIndex + 1) < currentTriangleVectorPtr->size() && (*currentTriangleVectorPtr)[curIndex + 1].bfsParentId == -1) {
					if (curIndex + 1) {
						(*currentTriangleVectorPtr)[curIndex + 1].bfsParentId = curIndex;
						idQueue.push(curIndex + 1);
					}
					else {
						(*currentTriangleVectorPtr)[curIndex + 1].bfsParentId == -2;
					}
				}
			}

			if ((curIndex - 1) >= 0 && (*currentTriangleVectorPtr)[curIndex - 1].bfsParentId == -1) {
				if (curIndex - 1) {
					(*currentTriangleVectorPtr)[curIndex - 1].bfsParentId = curIndex;
					idQueue.push(curIndex - 1);
				}
				else {
					(*currentTriangleVectorPtr)[curIndex - 1].bfsParentId == -2;
				}
			}
			if (!isLastRow(curIndex)) {
				if ((curIndex + collisionCols * 2 - 1) < currentTriangleVectorPtr->size() && (*currentTriangleVectorPtr)[curIndex + collisionCols * 2 - 1].bfsParentId == -1) {
					if (curIndex + collisionCols * 2 - 1) {
						(*currentTriangleVectorPtr)[curIndex + collisionCols * 2 - 1].bfsParentId = curIndex;
						idQueue.push(curIndex + collisionCols * 2 - 1);
					}
					else {
						(*currentTriangleVectorPtr)[curIndex + collisionCols * 2 - 1].bfsParentId == -2;
					}
				}
			}
		}
	}
	return false;
}

bool dfs(int from, int to, std::unordered_map<int, bool> &visited, int maxHeight) {
	visited[from] = true;
	if (from == to)
		return true;

	if (from % 2 == 0) {
		
		if (!visited[from + 1] && (from + 1) < currentTriangleVectorPtr->size() && dfs(from + 1, to, visited, maxHeight)) {
			(*currentTriangleVectorPtr)[from].collision = true;
			return true;
		}

		if (!isFirstCol(from)) {
			if (!visited[from - 1] && from -1 >= 0 && dfs(from - 1, to, visited, maxHeight)) {
				(*currentTriangleVectorPtr)[from].collision = true;
				return true;
			}
		}

		if (!isFirstRow(from)) {
			if (!visited[from - collisionCols * 2 + 1] && (from - collisionCols * 2 - 1 >= 0) && dfs(from - collisionCols * 2 + 1, to, visited, maxHeight)) {
				(*currentTriangleVectorPtr)[from].collision = true;
				return true;
			}
		}

	}
	else {

		if (!isLastCol(from)) {
			if (!visited[from + 1] && (from + 1) < currentTriangleVectorPtr->size() && dfs(from + 1, to, visited, maxHeight)) {
				(*currentTriangleVectorPtr)[from].collision = true;
				return true;
			}
		}

		if (!visited[from - 1] && (from - 1) >= 0 && dfs(from - 1, to, visited, maxHeight)) {
			(*currentTriangleVectorPtr)[from].collision = true;
			return true;
		}
		if (!isLastRow(from)) {
			if (!visited[from + collisionCols * 2 - 1] && (from + collisionCols * 2 - 1) < currentTriangleVectorPtr->size() && dfs(from + collisionCols * 2 - 1, to, visited, maxHeight)) {
				(*currentTriangleVectorPtr)[from].collision = true;
				return true;
			}
		}
	}
	return false;
}

void createPath(int maxHeight) {
	std::unordered_map<int, bool> visited;
	switch (operation) {
	case Operation::CollisionBfs:
		if (bfsCollision(source, dest, maxHeight)) {
			int to = dest;

			while (to != source) {
				(*currentTriangleVectorPtr)[to].collision = true;
				to = (*currentTriangleVectorPtr)[to].bfsParentId;
			}
		}
		return;
	case Operation::Bfs:
		if (bfs(source, dest)) {
			int to = dest;

			while (to != source) {
				(*currentTriangleVectorPtr)[to].collision = true;
				to = (*currentTriangleVectorPtr)[to].bfsParentId;
			}
		}
		return;
	case Operation::Dfs:
		dfs(source, dest, visited, maxHeight);
		return;
	default:
		return;
	}

	int to = dest;

	while (to != source) {
		(*currentTriangleVectorPtr)[to].collision = true;
		to = (*currentTriangleVectorPtr)[to].bfsParentId;
	}
}

void TrianglePrinter() {
	for (int i = 0; i < size; i++) {
		if (bRender)
			glBegin(GL_TRIANGLES);
		else
			glBegin(GL_LINES);
		Triangle cur = (*currentTriangleVectorPtr)[i];
		if (operationActive && cur.collision) {
			glColor3f(0, 0, 1);
			glVertex3f(cur.p1.x, cur.c1.z, cur.p1.y);
			glVertex3f(cur.p2.x, cur.c2.z, cur.p2.y);
			glVertex3f(cur.p3.x, cur.c3.z, cur.p3.y);
		}
		else {
			if (!operationActive && cur.isPicked) {
				glColor3f(1, 0, 0);
				glVertex3f(cur.p1.x, cur.c1.z, cur.p1.y);
				glVertex3f(cur.p2.x, cur.c2.z, cur.p2.y);
				glVertex3f(cur.p3.x, cur.c3.z, cur.p3.y);
			}
			else {
				glColor3f(cur.c1.r, cur.c1.g, cur.c1.b);
				glVertex3f(cur.p1.x, cur.c1.z, cur.p1.y);
				glColor3f(cur.c2.r, cur.c2.g, cur.c2.b);
				glVertex3f(cur.p2.x, cur.c2.z, cur.p2.y);
				glColor3f(cur.c3.r, cur.c3.g, cur.c3.b);
				glVertex3f(cur.p3.x, cur.c3.z, cur.p3.y);
			}
		}
		glEnd();
	}
}

void TriangleSpecialPrinter() {
	for (int i = 0; i < size; i++) {
		if (bRender)
			glBegin(GL_TRIANGLES);
		else
			glBegin(GL_LINE_STRIP);
		Triangle cur = currentTriangleVectorPtr->at(i);

		glColor3f(cur.left_r / 255.0f, cur.left_g / 255.0f, cur.left_b / 255.0f);
		glVertex3f(cur.p1.x, cur.c1.z, cur.p1.y);
		glVertex3f(cur.p2.x, cur.c2.z, cur.p2.y);
		glVertex3f(cur.p3.x, cur.c3.z, cur.p3.y);
		glEnd();
	}
}

void vectorPrinter() {
	if (leftClick) {
		TriangleSpecialPrinter();
		glFlush();
		glFinish();

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		POINT pos;
		GetCursorPos(&pos);

		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);

		GLint x = pos.x;
		GLint y = pos.y;
		glReadPixels(x, viewport[3]-y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
		int pickedID = data[0] + data[1] * 256 + data[2] * 256 * 256;
		if (operationActive) {
			leftClick = !leftClick;
			if (source == -1) {
				source = pickedID;
				(*currentTriangleVectorPtr)[pickedID].collision = true;
				
			}
			else if(dest == -1){
				dest = pickedID;
				(*currentTriangleVectorPtr)[pickedID].collision = true;
				std::unordered_map<int, bool> visited;
				int maxHeight = (*currentTriangleVectorPtr)[source].maxZ;
				if (maxHeight < (*currentTriangleVectorPtr)[dest].maxZ) {
					maxHeight = (*currentTriangleVectorPtr)[dest].maxZ;
				}
				createPath(maxHeight);
			}

		}
		else {
			leftClick = !leftClick;
			if (highRes) {
				vectorHighRes[pickedID].isPicked = !vectorHighRes[pickedID].isPicked;
				if (vectorHighRes[pickedID].isPicked) {
					pickingMap[vectorHighRes[pickedID].parentId]++;
				}
				else {
					pickingMap[vectorHighRes[pickedID].parentId]--;
				}

				if (pickingMap[vectorHighRes[pickedID].parentId] == 0)
					pickingMap.erase(vectorHighRes[pickedID].parentId);
			}
			else {
				vectorLowRes[pickedID].isPicked = !vectorLowRes[pickedID].isPicked;
				if (vectorLowRes[pickedID].isPicked) {
					pickingMap[pickedID] = 4;
				}
				else {
					pickingMap.erase(pickedID);
				}
			}
		}
		
	}
	else {
		TrianglePrinter();
	}
}

void updatePicks() {
	if (highRes) {
		for (Triangle& T : vectorLowRes) {
			T.isPicked = false;
		}

		for (Triangle& T : vectorHighRes) {
			if (pickingMap[T.parentId]) {
				T.isPicked = true;
			}
		}
	}
	else {
		for (Triangle& T : vectorHighRes) {
			T.isPicked = false;
		}
		for (Triangle& T : vectorLowRes) {
			if (pickingMap[T.vectorIndex]) {
				T.isPicked = true;
				pickingMap[T.vectorIndex] = 4;
			}
		}
	}

}

void cleanCollisions() {
	source = -1;
	dest = -1;
	for (int i = 0; i < size; i++) {
		(*currentTriangleVectorPtr)[i].collision = false;
		(*currentTriangleVectorPtr)[i].bfsParentId = -1;
	}
	collisionMap.clear();
}

int DrawGLScene(GLvoid)									
{
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(212, 60, 194, 0, 0, 0, 0, 1, 0);   // This Determines The Camera's Position And View
	//Triangle mid = vectorT[vectorT.size() / 2];
	//glTranslatef(mid.p1.x, mid.p1.y, mid.c1.z);                 // Move Left 1.5 Units And Into The Screen 6.0
	glScalef(scaleValue, scaleValue * HEIGHT_RATIO, scaleValue);
	glRotatef(xrot, 1.0f, 0.0f, 0.0f);                     // Rotate On The X Axis
	glRotatef(yrot, 0.0f, 1.0f, 0.0f);                     // Rotate On The Y Axis
	glRotatef(zrot, 0.0f, 0.0f, 1.0f);                     // Rotate On The Z Axis
	//glTranslatef(-mid.p1.x, -mid.p1.y, -mid.c1.z);      // Move Left 1.5 Units And Into The Screen 6.0
	vectorPrinter();									// Render The Height Map
	
	

	
	
	return TRUE;										// Keep Going
}

GLvoid KillGLWindow(GLvoid)								// Properly Kill The Window
{
	if (fullscreen)										// Are We In Fullscreen Mode?
	{
		ChangeDisplaySettings(NULL, 0);					// If So Switch Back To The Desktop
		ShowCursor(TRUE);								// Show Mouse Pointer
	}

	if (hRC)											// Do We Have A Rendering Context?
	{
		if (!wglMakeCurrent(NULL, NULL))					// Are We Able To Release The DC And RC Contexts?
		{
			MessageBox(NULL, "Release Of DC And RC Failed.", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
		}

		if (!wglDeleteContext(hRC))						// Are We Able To Delete The RC?
		{
			MessageBox(NULL, "Release Rendering Context Failed.", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
		}
		hRC = NULL;										// Set RC To NULL
	}

	if (hDC && !ReleaseDC(hWnd, hDC))					// Are We Able To Release The DC
	{
		MessageBox(NULL, "Release Device Context Failed.", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
		hDC = NULL;										// Set DC To NULL
	}

	if (hWnd && !DestroyWindow(hWnd))					// Are We Able To Destroy The Window?
	{
		MessageBox(NULL, "Could Not Release hWnd.", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
		hWnd = NULL;										// Set hWnd To NULL
	}

	if (!UnregisterClass("OpenGL", hInstance))			// Are We Able To Unregister Class
	{
		MessageBox(NULL, "Could Not Unregister Class.", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
		hInstance = NULL;									// Set hInstance To NULL
	}
}

/*	This Code Creates Our OpenGL Window.  Parameters Are:					*
 *	title			- Title To Appear At The Top Of The Window				*
 *	width			- Width Of The GL Window Or Fullscreen Mode				*
 *	height			- Height Of The GL Window Or Fullscreen Mode			*
 *	bits			- Number Of Bits To Use For Color (8/16/24/32)			*
 *	fullscreenflag	- Use Fullscreen Mode (TRUE) Or Windowed Mode (FALSE)	*/

BOOL CreateGLWindow(char* title, int width, int height, int bits, bool fullscreenflag)
{
	GLuint		PixelFormat;							// Holds The Results After Searching For A Match
	WNDCLASS	wc;										// Windows Class Structure
	DWORD		dwExStyle;								// Window Extended Style
	DWORD		dwStyle;								// Window Style
	RECT		WindowRect;								// Grabs Rectangle Upper Left / Lower Right Values
	WindowRect.left = (long)0;							// Set Left Value To 0
	WindowRect.right = (long)width;						// Set Right Value To Requested Width
	WindowRect.top = (long)0;								// Set Top Value To 0
	WindowRect.bottom = (long)height;						// Set Bottom Value To Requested Height

	fullscreen = fullscreenflag;							// Set The Global Fullscreen Flag

	hInstance = GetModuleHandle(NULL);		// Grab An Instance For Our Window
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	// Redraw On Size, And Own DC For Window.
	wc.lpfnWndProc = (WNDPROC)WndProc;			// WndProc Handles Messages
	wc.cbClsExtra = 0;							// No Extra Window Data
	wc.cbWndExtra = 0;							// No Extra Window Data
	wc.hInstance = hInstance;					// Set The Instance
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);	// Load The Default Icon
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);	// Load The Arrow Pointer
	wc.hbrBackground = NULL;							// No Background Required For GL
	wc.lpszMenuName = NULL;							// We Don't Want A Menu
	wc.lpszClassName = "OpenGL";						// Set The Class Name

	if (!RegisterClass(&wc))							// Attempt To Register The Window Class
	{
		MessageBox(NULL, "Failed To Register The Window Class.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;									// Return FALSE
	}

	if (fullscreen)										// Attempt Fullscreen Mode?
	{
		DEVMODE dmScreenSettings;						// Device Mode
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));	// Makes Sure Memory's Cleared
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);	// Size Of The Devmode Structure
		dmScreenSettings.dmPelsWidth = width;		// Selected Screen Width
		dmScreenSettings.dmPelsHeight = height;		// Selected Screen Height
		dmScreenSettings.dmBitsPerPel = bits;			// Selected Bits Per Pixel
		dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		// Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
		if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
			// If The Mode Fails, Offer Two Options.  Quit Or Use Windowed Mode.
			if (MessageBox(NULL, "The Requested Fullscreen Mode Is Not Supported By\nYour Video Card. Use Windowed Mode Instead?", "NeHe GL", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
			{
				fullscreen = FALSE;						// Windowed Mode Selected.  Fullscreen = FALSE
			}
			else
			{
				// Pop Up A Message Box Letting User Know The Program Is Closing.
				MessageBox(NULL, "Program Will Now Close.", "ERROR", MB_OK | MB_ICONSTOP);
				return FALSE;							// Return FALSE
			}
		}
	}

	if (fullscreen)										// Are We Still In Fullscreen Mode?
	{
		dwExStyle = WS_EX_APPWINDOW;						// Window Extended Style
		dwStyle = WS_POPUP;								// Windows Style
		ShowCursor(FALSE);								// Hide Mouse Pointer
	}
	else
	{
		dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;	// Window Extended Style
		dwStyle = WS_OVERLAPPEDWINDOW;					// Windows Style
	}

	AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);	// Adjust Window To True Requested Size

	// Create The Window
	if (!(hWnd = CreateWindowEx(dwExStyle,				// Extended Style For The Window
		"OpenGL",				// Class Name
		title,					// Window Title
		dwStyle |				// Defined Window Style
		WS_CLIPSIBLINGS |		// Required Window Style
		WS_CLIPCHILDREN,		// Required Window Style
		0, 0,					// Window Position
		WindowRect.right - WindowRect.left,	// Calculate Window Width
		WindowRect.bottom - WindowRect.top,	// Calculate Window Height
		NULL,					// No Parent Window
		NULL,					// No Menu
		hInstance,				// Instance
		NULL)))					// Dont Pass Anything To WM_CREATE
	{
		KillGLWindow();									// Reset The Display
		MessageBox(NULL, "Window Creation Error.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;									// Return FALSE
	}

	static	PIXELFORMATDESCRIPTOR pfd =					// pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),					// Size Of This Pixel Format Descriptor
		1,												// Version Number
		PFD_DRAW_TO_WINDOW |							// Format Must Support Window
		PFD_SUPPORT_OPENGL |							// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,								// Must Support Double Buffering
		PFD_TYPE_RGBA,									// Request An RGBA Format
		bits,											// Select Our Color Depth
		0, 0, 0, 0, 0, 0,								// Color Bits Ignored
		0,												// No Alpha Buffer
		0,												// Shift Bit Ignored
		0,												// No Accumulation Buffer
		0, 0, 0, 0,										// Accumulation Bits Ignored
		16,												// 16Bit Z-Buffer (Depth Buffer)  
		0,												// No Stencil Buffer
		0,												// No Auxiliary Buffer
		PFD_MAIN_PLANE,									// Main Drawing Layer
		0,												// Reserved
		0, 0, 0											// Layer Masks Ignored
	};

	if (!(hDC = GetDC(hWnd)))								// Did We Get A Device Context?
	{
		KillGLWindow();									// Reset The Display
		MessageBox(NULL, "Can't Create A GL Device Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;									// Return FALSE
	}

	if (!(PixelFormat = ChoosePixelFormat(hDC, &pfd)))		// Did Windows Find A Matching Pixel Format?
	{
		KillGLWindow();									// Reset The Display
		MessageBox(NULL, "Can't Find A Suitable PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;									// Return FALSE
	}

	if (!SetPixelFormat(hDC, PixelFormat, &pfd))			// Are We Able To Set The Pixel Format?
	{
		KillGLWindow();									// Reset The Display
		MessageBox(NULL, "Can't Set The PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;									// Return FALSE
	}

	if (!(hRC = wglCreateContext(hDC)))					// Are We Able To Get A Rendering Context?
	{
		KillGLWindow();									// Reset The Display
		MessageBox(NULL, "Can't Create A GL Rendering Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;									// Return FALSE
	}

	if (!wglMakeCurrent(hDC, hRC))						// Try To Activate The Rendering Context
	{
		KillGLWindow();									// Reset The Display
		MessageBox(NULL, "Can't Activate The GL Rendering Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;									// Return FALSE
	}

	ShowWindow(hWnd, SW_SHOW);							// Show The Window
	SetForegroundWindow(hWnd);							// Slightly Higher Priority
	SetFocus(hWnd);										// Sets Keyboard Focus To The Window
	ReSizeGLScene(width, height);						// Set Up Our Perspective GL Screen

	if (!InitGL())										// Initialize Our Newly Created GL Window
	{
		KillGLWindow();									// Reset The Display
		MessageBox(NULL, "Initialization Failed.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;									// Return FALSE
	}

	return TRUE;										// Success
}

LRESULT CALLBACK WndProc(HWND	hWnd,					// Handle For This Window
	UINT	uMsg,										// Message For This Window
	WPARAM	wParam,										// Additional Message Information
	LPARAM	lParam)										// Additional Message Information
{
	switch (uMsg)										// Check For Windows Messages
	{
	case WM_ACTIVATE:									// Watch For Window Activate Message
	{
		if (!HIWORD(wParam))							// Check Minimization State
		{
			active = TRUE;								// Program Is Active
		}
		else
		{
			active = FALSE;								// Program Is No Longer Active
		}

		return 0;										// Return To The Message Loop
	}

	case WM_SYSCOMMAND:									// Intercept System Commands
	{
		switch (wParam)									// Check System Calls
		{
		case SC_SCREENSAVE:								// Screensaver Trying To Start?
		case SC_MONITORPOWER:							// Monitor Trying To Enter Powersave?
			return 0;									// Prevent From Happening
		}
		break;											// Exit
	}

	case WM_CLOSE:										// Did We Receive A Close Message?
	{
		PostQuitMessage(0);								// Send A Quit Message
		return 0;										// Jump Back
	}

	case WM_RBUTTONDOWN:								// Did We Receive A Left Mouse Click?
	{
		bRender = !bRender;								// Change The Rendering State Between Fill And Wire Frame
		return 0;										// Jump Back
	}
	case WM_LBUTTONDOWN:								// Did We Receive A Left Mouse Click?
	{
		leftClick = !leftClick;							// Change The Rendering State Between Fill And Wire Frame
		return 0;										// Jump Back
	}

	case WM_KEYDOWN:									// Is A Key Being Held Down?
	{
		keys[wParam] = TRUE;							// If So, Mark It As TRUE
		return 0;										// Jump Back
	}

	case WM_KEYUP:										// Has A Key Been Released?
	{
		keys[wParam] = FALSE;							// If So, Mark It As FALSE
		return 0;										// Jump Back
	}

	case WM_SIZE:										// Resize The OpenGL Window
	{
		ReSizeGLScene(LOWORD(lParam), HIWORD(lParam));  // LoWord=Width, HiWord=Height
		return 0;										// Jump Back
	}
	}

	// Pass All Unhandled Messages To DefWindowProc
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE	hInstance,				// Instance
	HINSTANCE					hPrevInstance,			// Previous Instance
	LPSTR						lpCmdLine,				// Command Line Parameters
	int							nCmdShow)				// Window Show State
{
	MSG		msg;										// Windows Message Structure
	BOOL	done = FALSE;								// Bool Variable To Exit Loop

	// Ask The User Which Screen Mode They Prefer
	if (MessageBox(NULL, "Would You Like To Run In Fullscreen Mode?", "Start FullScreen?", MB_YESNO | MB_ICONQUESTION) == IDNO)
	{
		fullscreen = FALSE;								// Windowed Mode
	}
	// Create Our OpenGL Window
	if (!CreateGLWindow((char*)"Noam and Hadar's CS-Graphics project", 640, 480, 16, fullscreen))
	{
		return 0;										// Quit If Window Was Not Created
	}

	while (!done)										// Loop That Runs While done=FALSE
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))		// Is There A Message Waiting?
		{
			if (msg.message == WM_QUIT)					// Have We Received A Quit Message?
			{
				done = TRUE;								// If So done=TRUE
			}
			else										// If Not, Deal With Window Messages
			{
				TranslateMessage(&msg);					// Translate The Message
				DispatchMessage(&msg);					// Dispatch The Message
			}
		}
		else											// If There Are No Messages
		{
			// Draw The Scene.  Watch For ESC Key And Quit Messages From DrawGLScene()
			if ((active && !DrawGLScene()) || keys[VK_ESCAPE])	// Active?  Was There A Quit Received?
			{
				done = TRUE;								// ESC or DrawGLScene Signalled A Quit
			}
			else if (active)							// Not Time To Quit, Update Screen
			{
				SwapBuffers(hDC);						// Swap Buffers (Double Buffering)
			}

			if (keys[VK_F1])							// Is F1 Being Pressed?
			{
				keys[VK_F1] = FALSE;						// If So Make Key FALSE
				KillGLWindow();							// Kill Our Current Window
				fullscreen = !fullscreen;					// Toggle Fullscreen / Windowed Mode
				// Recreate Our OpenGL Window
				if (!CreateGLWindow((char*)"Noam and Hadar's CS-Graphics project", 640, 480, 16, fullscreen))
				{
					return 0;							// Quit If Window Was Not Created
				}
			}

			if (keys[VK_UP])							// Is the UP ARROW key Being Pressed?
				scaleValue += 0.001f;					// Increase the scale value to zoom in

			if (keys[VK_DOWN])							// Is the DOWN ARROW key Being Pressed?
				scaleValue -= 0.001f;					// Decrease the scale value to zoom out

			if (keys[0x42]) { //BFS - code B
				if (!operationActive) {
					operationActive = true;
					operation = Operation::Bfs;
					if (highRes) {
						currentTriangleVectorPtr = &collisionVector;
						collisionCols = highResCols;
						collisionRows = highResRows;
					}
					else {
						collisionCols = lowResCols;
						collisionRows = lowResRows;
					}
				}
				else {
					operationActive = false;
					operation = Operation::Empty;
					cleanCollisions();
					if (highRes) {
						currentTriangleVectorPtr = &vectorHighRes;
					}

				}
				keys[0x42] = false;
			}
			if (keys[0x43]) { //Collision - code C
				if (!operationActive) {
					operationActive = true;
					operation = Operation::CollisionBfs;
					if (highRes) {
						currentTriangleVectorPtr = &collisionVector;
						collisionCols = highResCols;
						collisionRows = highResRows;
					}
					else {
						collisionCols = lowResCols;
						collisionRows = lowResRows;
					}
				}
				else {
					operationActive = false;
					operation = Operation::Empty;
					cleanCollisions();
					if (highRes) {
						currentTriangleVectorPtr = &vectorHighRes;
					}

				}
				keys[0x43] = false;
			}
			if (keys[0x44]) { //DFS - code D
				if (!operationActive) {
					operationActive = true;
					operation = Operation::Dfs;
					if (highRes) {
						currentTriangleVectorPtr = &collisionVector;
						collisionCols = highResCols;
						collisionRows = highResRows;
					}
					else {
						collisionCols = lowResCols;
						collisionRows = lowResRows;
					}
				}
				else {
					operationActive = false;
					operation = Operation::Empty;
					cleanCollisions();
					if (highRes) {
						currentTriangleVectorPtr = &vectorHighRes;
					}

				}
				keys[0x44] = false;
			}
			if (keys[0x4C]) {  // change to low resolution
				if (highRes) { 
					highRes = false;
					size = vectorLowRes.size();
					currentTriangleVectorPtr = &vectorLowRes;
					updatePicks();
				}
				keys[0x4C] = false;
			}
			if (keys[0x48]) { // change to high resolution
				if (!highRes) {
					highRes = true;
					size = vectorHighRes.size();
					currentTriangleVectorPtr = &vectorHighRes;
					updatePicks();
				}
				keys[0x48] = false;
			}
			
			if (keys[VK_LEFT])			
				yrot += 0.1f;
			if (keys[VK_RIGHT])		
				yrot -= 0.1f;					
			if (keys[0x5A])	//z						
				zrot -= 0.1f;					
			if (keys[0x58]) // x
				zrot += 0.1f;
			if (keys[0x4E]) // m
				xrot += 0.1f;
			if (keys[0x4D]) // n
				xrot -= 0.1f;
		}
	}

	// Shutdown
	KillGLWindow();										// Kill The Window
	return (msg.wParam);								// Exit The Program
}