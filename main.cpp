//          Copyright Luca R. L. de Carvalho 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

// IMPORTANT: whenever dealing with camera coordinates,
// we'll actually be dealing with the inverted coords, 
// as the camera is actually static, and the world itself
// moves arround it.

#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <vector>
#include <map>
#include <algorithm>
#include <gl/glut.h>

#define WINDOW_W 800
#define WINDOW_H 600
#define MOUSE_SENSITIVITY 0.4

using namespace std;

///////////
// Classes

class Point3 {
public:
	float x = 0, y = 0, z = 0;
	
	Point3(float x, float y, float z) {
		this->x = x;
		this->y = y;
		this->z = z;
	}
	Point3() {
	}
};

class Face {
public:
	int vertexIds[4];
	int normalIds[4];
};

class Obj {
public:
	string name;
	vector<Point3> vertices = vector<Point3>();
	vector<Point3> normals = vector<Point3>();
	vector<Face> faces = vector<Face>();

	Obj(const char* filename) {
		readFile(filename);
	}

	Obj() {
	}

	void readFile(const char* filename) {
		// Reads a single object

		string line;
		ifstream file;

		file.open(filename, ifstream::in);

		while (getline(file, line)) {

			// Ignore comments
			if (line[0] == '#')
				continue;
			else if (line[0] == 'o') {
				// name
				this->name = line.substr(1);
			}
			else if (line[0] == 'v') {
				Point3 point;
				if (line[1] == 'n') {
					// normal vector
					sscanf_s(line.c_str(), "vn %f %f %f", &point.x, &point.y, &point.z);
					this->normals.push_back(point);
				}
				else {
					// vertex
					sscanf_s(line.c_str(), "v %f %f %f", &point.x, &point.y, &point.z);
					this->vertices.push_back(point);
				}

				cout << "v/vn: " << point.x << " " << point.y << " " << point.z << endl;
			}
			else if (line[0] == 'f') {
				// face
				Face face;
				sscanf_s(line.c_str(), "f %d//%d %d//%d %d//%d %d//%d",
					&face.vertexIds[0], &face.normalIds[0], &face.vertexIds[1], &face.normalIds[1],
					&face.vertexIds[2], &face.normalIds[2], &face.vertexIds[3], &face.normalIds[3]);
				this->faces.push_back(face);

				cout << "f: " << face.vertexIds[0] << " " << face.vertexIds[1] << " " << face.vertexIds[2] << " " << face.vertexIds[3] << endl;
			}
			else {
				cout << "Invalid syntax or unsupported parameter: " << line << endl;
				//break;
			}
		}

		cout << this->name << endl;
	}

	void toBuffer() {
		// Transfers the object to OpenGL's buffer

		glBegin(GL_QUADS);
		for (int i = 0; i < this->faces.size(); i++) {
			for (int j = 0; j < 4; j++) {
				Point3 vertex = this->vertices[this->faces[i].vertexIds[j] - 1];
				Point3 normal = this->normals[this->faces[i].normalIds[j] - 1];
				glNormal3f(normal.x, normal.y, normal.z);
				glVertex3f(vertex.x, vertex.y, vertex.z);
			}
		}
		glEnd();
	}
};

////////////////////
// Global variables
GLfloat fovY, fAspect, cameraRotationY;
Obj* object;
map<string, Obj> objects = map<string, Obj>();
Point3* cameraPos; // Actually, stores the inverted coordinates
Point3* cameraLookAt;
int timeSinceStart;
float deltaTimeSec = 0;

///////////////////////
// Function prototypes
void init();
void draw();
void idle();
void reshapeWindow(GLsizei w, GLsizei h);
void setVisualizationParameters();
void handleKeyboard(unsigned char key, int x, int y);
void handleMouseMotion(int x, int y);
Point3* getCameraForward();
void correctForBoundaries();
void teleportIfNecessary();
float clampFloat(float value, float min, float max);
bool between(float value, float min, float max);

/////////////
// Functions
int main() {
	objects.insert({ "bottom", Obj("mezzanine_bottom.obj") });
	objects.insert({ "stairs", Obj("mezzanine_stairs.obj") });
	objects.insert({ "top", Obj("mezzanine_top.obj") });

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);

	glutInitWindowSize(WINDOW_W, WINDOW_H);
	glutCreateWindow("Mezzanine - Luca Carvalho");

	glutDisplayFunc(draw);
	glutIdleFunc(idle);
	glutReshapeFunc(reshapeWindow);
	glutKeyboardFunc(handleKeyboard);
	glutPassiveMotionFunc(handleMouseMotion);
	glutSetCursor(GLUT_CURSOR_NONE);

	init();

	glutMainLoop();
	
	return 0;
}

void init() {
	GLfloat ambientLight[4] = { 0.2, 0.2, 0.2, 1.0 };
	GLfloat diffuseLight[4] = { 0.7, 0.7, 0.7, 1.0 };
	GLfloat specularLight[4] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat lightPosition[4] = { 0, 100, 0, 1.0 };

	GLfloat specularity[4] = { 1.0, 1.0, 1.0, 1.0 };
	GLint specMaterial = 60;

	glMaterialfv(GL_FRONT, GL_SPECULAR, specularity);
	glMateriali(GL_FRONT, GL_SHININESS, specMaterial);

	glClearColor(0.1, 0.1, 0.1, 1);
	glShadeModel(GL_SMOOTH);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientLight);

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);

	fovY = 45;

	cameraPos = new Point3(0, -2, 0);
	cameraLookAt = new Point3(0, 2, 20);
	cameraRotationY = 0;

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(-cameraPos->x, -cameraPos->y, -cameraPos->z, 0, 2, 20, 0, 1, 0);

	timeSinceStart = glutGet(GLUT_ELAPSED_TIME);
}

void idle() {
	/*float currentTime = glutGet(GLUT_ELAPSED_TIME);
	
	deltaTimeSec = (currentTime - timeSinceStart) / 1000;
	timeSinceStart = currentTime;


	cout << deltaTimeSec << endl;*/
}

void draw() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glColor3f(0.5, 0.5, 1);
	objects.find("bottom")->second.toBuffer();

	glColor3f(0.5, 0.5, 0.5);
	objects.find("stairs")->second.toBuffer();

	glColor3f(0.5, 1, 0.5);
	objects.find("top")->second.toBuffer();

	//glPushMatrix();
	//glTranslatef(0, 0, 0);
	//object->toBuffer();
	//glPopMatrix();

	glutSwapBuffers();
}

void reshapeWindow(GLsizei w, GLsizei h) {
	if (h == 0) h = 1;
	glViewport(0, 0, w, h);

	fAspect = (GLfloat)w / (GLfloat)h;

	setVisualizationParameters();
}

void setVisualizationParameters() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fovY, fAspect, 0.1, 500);
}

void handleKeyboard(unsigned char key, int x, int y) {
	Point3* forward = getCameraForward();

	glMatrixMode(GL_MODELVIEW);

	switch (key){
		case 'w':
			cameraPos->x += forward->x;
			cameraPos->z += forward->z;
			glTranslatef(forward->x, 0, forward->z);
			break;
		case 's':
			cameraPos->x -= forward->x;
			cameraPos->z -= forward->z;
			glTranslatef(-forward->x, 0, -forward->z);
			break;
		case 'a':
			cameraPos->x += forward->z;
			cameraPos->z -= forward->x;
			glTranslatef(forward->z, 0, -forward->x);
			break;
		case 'd':
			cameraPos->x -= forward->z;
			cameraPos->z += forward->x;
			glTranslatef(-forward->z, 0, +forward->x);
			break;
		case 'q':
			exit(0);
		default:
			cout << "Key: " << key << endl;
			break;

	}

	correctForBoundaries();
	teleportIfNecessary();

	glutPostRedisplay();

	cout << "Camera: " << cameraPos->x << ", " << cameraPos->y << ", "<< cameraPos->z << endl;
	cout << "Facing: " << forward->x << ", " << forward->y << ", " << forward->z << endl;
	cout << "Rotation: " << cameraRotationY << endl;
}

void handleMouseMotion(int x, int y) {
	static int prevX = 0, prevY = 0;

	glMatrixMode(GL_MODELVIEW);

	if (x < prevX) {
		// Mouse moved to the left
		cameraRotationY = cameraRotationY >= 359 ? 0 : cameraRotationY + 1;

		glTranslatef(-cameraPos->x, -cameraPos->y, -cameraPos->z);
		glRotatef(-MOUSE_SENSITIVITY, 0, 1, 0);
		glTranslatef(cameraPos->x, cameraPos->y, cameraPos->z);
	}
	else {
		// Mouse moved to the right
		cameraRotationY = cameraRotationY <= 0 ? 359 : cameraRotationY - 1;

		glTranslatef(-cameraPos->x, -cameraPos->y, -cameraPos->z);
		glRotatef(+MOUSE_SENSITIVITY, 0, 1, 0);
		glTranslatef(cameraPos->x, cameraPos->y, cameraPos->z);
	}

	if (x > 600)
		glutWarpPointer(400, 100);
	else if (x < 100)
		glutWarpPointer(400, 100);


	glutPostRedisplay();

	prevX = x;
	prevY = y;
}

Point3* getCameraForward() {
	Point3* forward;
	GLfloat mat[16];

	glGetFloatv(GL_MODELVIEW_MATRIX, mat);
	forward = new Point3(-mat[8], mat[9], mat[10]);

	/*cout << "mat: " << endl;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			printf("%10.6f", mat[i * 4 + j]);
		}
		cout << endl;
	}
	cout << "=====" << endl;*/

	return forward;
}

void correctForBoundaries() {
	glMatrixMode(GL_MODELVIEW);
	glTranslatef(-cameraPos->x, -cameraPos->y, -cameraPos->z);

	// Base
	cameraPos->x = clampFloat(cameraPos->x, -11.5, 11.5);
	cameraPos->z = clampFloat(cameraPos->z, -10, 10);

	// Mezzanine
	if (cameraPos->y < -7.53) {
		if (between(cameraPos->z, -10, -4.64)) {
			// in front of the stairs
			cameraPos->x = clampFloat(cameraPos->x, -5.45, 11.5);
			if (between(cameraPos->x, -5.45, 4.5)) {
				// beside the hole
				cameraPos->z = clampFloat(cameraPos->z, -10, -4.64);
			}
		}
		else if (between(cameraPos->z, 4.76, 10)) {
			// opposite to the first stretch
			cameraPos->x = clampFloat(cameraPos->x, -2.6, 11.5);
			if (between(cameraPos->x, -2.6, 4.5)) {
				// beside the hole
				cameraPos->z = clampFloat(cameraPos->z, 4.76, 10);
			}
		}
		else if (between(cameraPos->z, -4.64, 4.76)) {
			// in front of the hole
			cameraPos->x = clampFloat(cameraPos->x, 4.5, 11.5);
		}
	}

	glTranslatef(cameraPos->x, cameraPos->y, cameraPos->z);
}

void teleportIfNecessary() {
	glMatrixMode(GL_MODELVIEW);

	glTranslatef(-cameraPos->x, -cameraPos->y, -cameraPos->z);

	// Lower stair step -> upper floor
	if (between(cameraPos->x, -11.5, -7.5)
		&& between(cameraPos->z, -2.5, -1.5)
		&& between(cameraPos->y, -2.01, -1.99)) {

		cameraPos->x = 1.86;
		cameraPos->y = -7.54;
		cameraPos->z = -9.9;
	}

	// Upper stair step -> lower floor
	if (between(cameraPos->x, -3.32, -2.32)
		&& between(cameraPos->z, -10, -7.5)
		&& between(cameraPos->y, -7.55, -7.53)) {

		cameraPos->x = -9.35;
		cameraPos->y = -2;
		cameraPos->z = 0;
	}

	glTranslatef(cameraPos->x, cameraPos->y, cameraPos->z);
}

float clampFloat(float value, float min, float max) {
	if (value < min)
		return min;
	if (value > max)
		return max;
	return value;
}

bool between(float value, float min, float max) {
	return value >= min && value <= max;
}