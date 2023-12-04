#define NOMINMAX
#include <iostream>
#include <memory>
#include <filesystem>
#include <algorithm>
#include "lsystem.hpp"
#include <GL/freeglut.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

namespace fs = std::filesystem;

// Menu identifiers
const int MENU_OBJBASE = 64;				// Select L-System to view
const int MENU_PREVITER = 2;				// Show previous iteration
const int MENU_NEXTITER = 3;				// Show next iteration
const int MENU_REPARSE = 4;					// Re-parse the last loaded file
const int MENU_EXIT = 1;					// Exit application
std::vector<std::string> modelFilenames;	// Paths to L-System files to load

// OpenGL state
int width, height;
std::unique_ptr<LSystem> lsystem;
unsigned int iter = 0;
std::string lastFilename;
int lastFilenameIdx = -1;

float ang = 0;
glm::vec3 axis = glm::vec3(1.f);

// Initialization functions
void initGLUT(int* argc, char** argv);
void initMenu();
void findModelFiles();

// Callback functions
void display();
void reshape(GLint width, GLint height);
void keyPress(unsigned char key, int x, int y);
void keyRelease(unsigned char key, int x, int y);
void keySpecial(int key, int x, int y);
void mouseBtn(int button, int state, int x, int y);
void mouseMove(int x, int y);
void idle();
void menu(int cmd);
void cleanup();

// Program entry point
int main(int argc, char** argv) {
	std::string configFile = "models/Cherry Blossom.txt";
	if (argc > 1)
		configFile = std::string(argv[1]);

	try {
		// Create the window and menu
		initGLUT(&argc, argv);
		initMenu();

		// OpenGL settings
		glClearColor(1.f, 1.f, 1.f, 1.f);
		//glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClearDepth(1.0f);
		glEnable(GL_DEPTH_TEST);

		// Create L-System object
		lsystem.reset(new LSystem);
		try {
			if (!configFile.empty()) {
				lsystem->parseFile(configFile);
				lastFilename = configFile;

				for (unsigned int i = 0; i < modelFilenames.size(); i++) {
					if (configFile == modelFilenames[i])
						lastFilenameIdx = i;
				}

				iter = lsystem->getNumIter() - 1;
				std::cout << "Iteration " << iter << std::endl;
			}
		} catch (const std::exception& e) {
			std::cerr << "Parse error: " << e.what() << std::endl;
		}

	}
	catch (const std::exception& e) {
		// Handle any errors
		std::cerr << "Fatal error: " << e.what() << std::endl;
		cleanup();
		return -1;
	}

	// Execute main loop
	glutMainLoop();

	return 0;
}

// Setup window and callbacks
void initGLUT(int* argc, char** argv) {
	// Set window and context settings
	width = 800; height = 600;
	glutInit(argc, argv);
	glutInitWindowSize(width, height);
	glutInitContextVersion(3, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE);
	// Create the window
	glutCreateWindow("FreeGLUT Window");

	// Create a menu

	// GLUT callbacks
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyPress);
	glutKeyboardUpFunc(keyRelease);
	glutSpecialFunc(keySpecial);
	glutMouseFunc(mouseBtn);
	glutMotionFunc(mouseMove);
	glutIdleFunc(idle);
	glutCloseFunc(cleanup);
}

void initMenu() {
	// Create a submenu with all the objects you can view
	findModelFiles();
	int objMenu = glutCreateMenu(menu);
	for (int i = 0; i < modelFilenames.size(); i++) {
		glutAddMenuEntry(modelFilenames[i].c_str(), MENU_OBJBASE + i);
	}

	// Create the main menu, adding the objects menu as a submenu
	glutCreateMenu(menu);
	glutAddSubMenu("View L-System", objMenu);
	glutAddMenuEntry("Prev iter", MENU_PREVITER);
	glutAddMenuEntry("Next iter", MENU_NEXTITER);
	glutAddMenuEntry("Reparse", MENU_REPARSE);
	glutAddMenuEntry("Exit", MENU_EXIT);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

}

void findModelFiles() {
	// Search the models/ directory for any file ending in .obj
	fs::path modelsDir = "models";
	for (auto& di : fs::directory_iterator(modelsDir)) {
		if (di.is_regular_file() && di.path().extension() == ".txt")
			modelFilenames.push_back(di.path().string());
	}
	std::sort(modelFilenames.begin(), modelFilenames.end());
}

// Called whenever a screen redraw is requested
void display() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float aspect = (float)width / (float)height;
	glm::mat4 proj(1.0f);

	proj = proj * glm::ortho(-1.f, 1.f, -1.f, 1.f, -100.f, 100.f);
	//glm::perspective(90.0f, 1.0f, 0.001f, 1000.0f);
// Draw the L-System
	if (lsystem && lsystem->getNumIter() > 0)
		lsystem->drawIter(iter, proj, glm::rotate(ang, axis));

	// Scene is rendered to the back buffer, so swap the buffers to display it
	glutSwapBuffers();
}

// Called when the window is resized
void reshape(GLint w, GLint h) {
	// Tell OpenGL the new window size
	width = w; height = h;
	glViewport(0, 0, w, h);
}

// Called when a key is pressed
void keyPress(unsigned char key, int x, int y) {
	switch (key) {
	case ' ':
		menu(MENU_REPARSE);
		break;
	case 'q':
		if (axis != glm::vec3(1.f, 0.f, 0.f)) {
			ang = .15;
			axis = glm::vec3(1.f, 0.f, 0.f);
		}
		else {
			ang += .15;
		}
		glutPostRedisplay();
		break;
	case 'w':
		if (axis != glm::vec3(0.f, 1.f, 0.f)) {
			ang = .15;
			axis = glm::vec3(0.f, 1.f, 0.f);
		}
		else {
			ang += .15;
		}
		glutPostRedisplay();
		break;
	case 'e':
		if (axis != glm::vec3(0.f, 0.f, 1.f)) {
			ang = .15;
			axis = glm::vec3(0.f, 0.f, 1.f);
		}
		else {
			ang += .15;
		}
		glutPostRedisplay();
		break;
	case 't':
		lsystem->angle1 += 1;
		lsystem->update();
		glutPostRedisplay();
		printf("angle1: %f\n", lsystem->angle1);
		break;
	case 'g':
		lsystem->angle1 -= 1;
		lsystem->update();
		glutPostRedisplay();
		printf("angle1: %f\n", lsystem->angle1);
		break;
	case 'r':
		lsystem->angle2 += 5;
		lsystem->update();
		glutPostRedisplay();
		printf("angle2: %f\n", lsystem->angle2);
		break;
	case 'f':
		lsystem->angle2 -= 5;
		lsystem->update();
		glutPostRedisplay();
		printf("angle2: %f\n", lsystem->angle2);
		break;
	}
}

// Called when a key is released
void keyRelease(unsigned char key, int x, int y) {
	switch (key) {
	case 27:	// Escape key
		menu(MENU_EXIT);
		break;
	}
}

// Called for special keys like arrow keys
void keySpecial(int key, int x, int y) {
	switch (key) {
	// Previous iteration
	case GLUT_KEY_LEFT:
		menu(MENU_PREVITER);
		break;
	// Next iteration
	case GLUT_KEY_RIGHT:
		menu(MENU_NEXTITER);
		break;
	// Previous object
	case GLUT_KEY_UP: {
		int idx = lastFilenameIdx;
		if (idx < 0) idx = 0;
		idx--;
		idx = (idx + modelFilenames.size()) % modelFilenames.size();
		menu(MENU_OBJBASE + idx);
		break; }
	// Next object
	case GLUT_KEY_DOWN: {
		int idx = lastFilenameIdx;
		idx++;
		idx = idx % modelFilenames.size();
		menu(MENU_OBJBASE + idx);
		break; }
	}
}

// Called when a mouse button is pressed or released
void mouseBtn(int button, int state, int x, int y) {}

// Called when the mouse moves
void mouseMove(int x, int y) {}

// Called when there are no events to process
//static auto start = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());  // record start time
void idle() {
	// TODO: anything that happens every frame (e.g. movement) should be done here
	// Be sure to call glutPostRedisplay() if the screen needs to update as well
	//auto finish = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());  // record end time
	//auto elapsed = static_cast<float>((finish - start).count());
	// NOTE: keep refreshing the screen
	//if ((int)elapsed % 300 == 0) { glutPostRedisplay(); }
}

// Called when a menu button is pressed
void menu(int cmd) {
	switch (cmd) {
	// End the program
	case MENU_EXIT:
		glutLeaveMainLoop();
		break;

	// Display previous iteration
	case MENU_PREVITER:
		if (iter != 0) {
			iter--;
			std::cout << "Iteration " << iter << std::endl;
			glutPostRedisplay();
		}
		break;

	// Display next iteration
	case MENU_NEXTITER:
		if (!lsystem->getNumIter()) break;
		try {
			if (iter + 1 >= lsystem->getNumIter())
				lsystem->iterate();
			iter++;
			std::cout << "Iteration " << iter << std::endl;
			glutPostRedisplay();
		} catch (const std::exception& e) {
			std::cerr << "Too many iterations: " << e.what() << std::endl;
		}
		break;

	// Re-parse last loaded file
	case MENU_REPARSE:
		if (!lastFilename.empty()) {
			try {
				lsystem->parseFile(lastFilename);
				iter = lsystem->getNumIter() - 1;
				std::cout << "Iteration " << iter << std::endl;
				glutPostRedisplay();
			} catch (const std::exception& e) {
				std::cerr << "Parse error: " << e.what() << std::endl;
			}
		}
		break;

	default:
		// Show the other objects
		if (cmd >= MENU_OBJBASE) {
			try {
				lsystem->parseFile(modelFilenames[cmd - MENU_OBJBASE]);
				lastFilename = modelFilenames[cmd - MENU_OBJBASE];
				lastFilenameIdx = cmd - MENU_OBJBASE;
				iter = lsystem->getNumIter() - 1;
				std::cout << "Iteration " << iter << std::endl;
				glutPostRedisplay();	// Request redraw
			} catch (const std::exception& e) {
				std::cerr << "Parse error: " << e.what() << std::endl;
			}
		}
		break;
	}
}

// Called when the window is closed or the event loop is otherwise exited
void cleanup() {
	lsystem.reset(nullptr);
}
