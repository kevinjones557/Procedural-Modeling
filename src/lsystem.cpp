#define NOMINMAX
#include "lsystem.hpp"
#include <fstream>
#include <sstream>
#include <stack>
#include <random>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include "util.hpp"

// Stream processing helper functions
std::stringstream preprocessStream(std::istream& istr);
std::string getNextLine(std::istream& istr);
std::string trim(const std::string& line);

// Static L-System members
unsigned int LSystem::refcount = 0;
GLuint LSystem::shader = 0;
GLuint LSystem::xformLoc = 0;

int getRandomNumber(int n) {
	std::random_device rd;  // Create a random device
	std::mt19937 gen(rd()); // Create a random number generator with the random device as a seed

	std::uniform_int_distribution<int> distribution(0, n); // Create a uniform distribution from 0 to n

	return distribution(gen); // Generate a random number within the specified range
}


// Constructor
LSystem::LSystem() :
	angle1(0.0f),
	angle2(0.0f),
	trunk(0),
	branch(0),
	twig(0),
	vao(0),
	vbo(0),
	bufSize(0) {

	// Create shader if we're the first object
	if (refcount == 0)
		initShader();
	refcount++;
}

// Destructor
LSystem::~LSystem() {
	// Destroy vertex buffer and array
	if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
	if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
	bufSize = 0;

	refcount--;
	// Destroy shader if we're the last object
	if (refcount == 0) {
		if (shader) { glDeleteProgram(shader); shader = 0; }
	}
}

// Move constructor
LSystem::LSystem(LSystem&& other) :
	strings(std::move(other.strings)),
	rules(std::move(other.rules)),
	angle1(other.angle1),
	angle2(other.angle2),
	trunk(other.trunk),
	branch(other.branch),
	twig(other.twig),
	vao(other.vao),
	vbo(other.vbo),
	iterData(std::move(other.iterData)),
	bufSize(other.bufSize) {

	other.vao = 0;
	other.vbo = 0;
	other.bufSize = 0;
	// Increment reference count (temp will decrement upon destructor)
	refcount++;
}

// Move assignment operator
LSystem& LSystem::operator=(LSystem&& other) {
	strings = std::move(other.strings);
	rules = std::move(other.rules);
	angle1 = other.angle1;
	angle2 = other.angle2;
	iterData = std::move(other.iterData);
	bufSize = other.bufSize;
	trunk = other.trunk;
	branch = other.branch;
	twig = other.twig;

	// Release any existing buffers
	if (vao) { glDeleteVertexArrays(1, &vao); }
	if (vbo) { glDeleteBuffers(1, &vbo); }
	// Acquire other's buffers
	vao = other.vao;
	vbo = other.vbo;

	other.vao = 0;
	other.vbo = 0;
	other.bufSize = 0;
	// Refcount stays the same

	return *this;
}

// Parse input stream and replace current L-System with contents
// Assumes valid input has no comments and ends with a newline character
void LSystem::parse(std::istream& istr) {
	// Temporary storage as input stream is parsed
	float inAngle1 = 0.0f;
	float inAngle2 = 0.0f;
	unsigned int inIters = 0;
	std::string inAxiom;
	std::map<char, std::vector<Data>> inRules;

	char *buffer = new char[1000];
	int count = 1;
	while (istr.getline(buffer, 999)) {
		std::string str = buffer;
		str.erase(remove_if(str.begin(), str.end(), isspace), str.end());
		if (count == 1) {
			inAngle1 = std::stod(str);
		}
		else if (count == 2) {
			inAngle2 = std::stod(str);
		}
		else if (count == 3) {
			inIters = std::stod(str);
		}
		else if (count == 4) {
			inAxiom = str;
		}
		else {
			char c = str[0];
			double p = 1;
			std::string rule;
			if (str[1] == ':') {
				rule = str.substr(2);
			}
			else {
				char* probability = new char[50];
				int i = 1;
				while (str[i] != ':') {
					probability[i - 1] = str[i];
					i++;
				}
				p = std::stod(probability);
				rule = str.substr(i + 1);
			}
			auto pos = inRules.find(c);
			Data d = { p, rule };
			if (pos == inRules.end()) {
				std::vector<Data> v;
				v.push_back(d);
				inRules.insert({ c, v });
			}
			else
			{
				pos->second.push_back(d);
			}
		}
		count++;
	}



	// Make your changes above this line
	// END TODO ===============================================================


	// Replace current state with parsed contents
	angle1 = inAngle1;
	angle2 = inAngle2;
	strings = { inAxiom };
	rules = std::move(inRules);
	// Create geometry for axiom
	iterData.clear();
	auto verts = createGeometry(strings.back());
	addVerts(verts);

	// Perform iterations
	try {
		while (strings.size() < inIters)
			iterate();
	} catch (const std::exception& e) {
		// Failed to iterate, stop at last iter
		std::cerr << "Too many iterations: geometry exceeds maximum buffer size" << std::endl;
	}
}

// Parse contents of source string
void LSystem::parseString(std::string string) {
	std::stringstream ss(string);

	// Preprocess to remove comments & whitespace
	ss = preprocessStream(ss);
	parse(ss);
}

// Parse a file
void LSystem::parseFile(std::string filename) {
	std::ifstream file(filename);
	if (!file.is_open())
		throw std::runtime_error("failed to open " + filename);

	// Preprocess to remove comments & whitespace
	std::stringstream ss = preprocessStream(file);
	parse(ss);
}

// Apply rules to the latest string to generate the next string
unsigned int LSystem::iterate() {
	if (strings.empty()) return 0;

	// Apply rules to last string
	std::string newString = applyRules(strings.back());
	// Get geometry of new iteration
	auto verts = createGeometry(newString);

	// Check for too-large buffer
	auto& id = iterData.back();
	if ((id.first + id.count + verts.size()) * sizeof(glm::vec2) > MAX_BUF)
		throw std::runtime_error("geometry exceeds maximum buffer size");


	// Store new iteration
	strings.push_back(newString);
	addVerts(verts);

	return getNumIter();
}

// Draw the latest iteration of the L-System
void LSystem::draw(glm::mat4 viewProj) {
	if (!getNumIter()) return;
	drawIter(getNumIter() - 1, viewProj);
}

// Draw a specific iteration of the L-System
void LSystem::drawIter(unsigned int iter, glm::mat4 viewProj) {
	IterData& id = iterData.at(iter);

	glUseProgram(shader);
	glBindVertexArray(vao);

	static float animation_iter = 0;
	animation_iter += .1;
	glm::mat4 rotMat = glm::rotate(animation_iter, glm::vec3(0.0, 1.0, 0.0));

	// Send matrix to shader
	glm::mat4 xform = viewProj * rotMat * id.bbfix;
	glUniformMatrix4fv(xformLoc, 1, GL_FALSE, glm::value_ptr(xform));
	// Draw L-System

	glLineWidth(10.f);

	glDrawArrays(GL_LINES, id.first, trunk);

	glLineWidth(5.f);

	glDrawArrays(GL_LINES, id.first + trunk, branch);

	glLineWidth(5.f);

	glDrawArrays(GL_LINES, id.first + trunk +  branch, twig);

	glLineWidth(2.f);

	glDrawArrays(GL_LINES, id.first + trunk + branch + twig, id.count - trunk - branch - twig);

	//int n = 10;
	//for (int i = 1; i <= n; i++) {
	//	glLineWidth(n + 1 - i);
	//	glDrawArrays(GL_LINES, id.first + (i - 1) * id.count / n, id.count / n);
	//}

	glBindVertexArray(0);
	glUseProgram(0);
}

// Apply rules to a given string and return the result
std::string LSystem::applyRules(std::string string) {

	std::string newstr = "";
	for (char c : string) {
		srand((unsigned)time(NULL));
		int random = getRandomNumber(1000);
		auto pos = rules.find(c);
		if (pos != rules.end()) {
			double tot_prob = 0;
			for (Data d : pos->second) {
				tot_prob += d.prob;
			}
			double max = 0;
			for (Data d : pos->second) {
				max += 1000 * d.prob / tot_prob;
				if (random <= max) {
					newstr += d.rule;
					break;
				}
			}
		}
		else
		{
			newstr += c;	
		}
	}
	printf("string: %s\n", newstr.c_str());

	// Replace this line with your implementation
	return newstr;
}

glm::mat3 LSystem::rotate(const float degree, const int axis) {
	// degree: rotation degree
	// axis: which axis to rotate around

	glm::mat3 res = glm::mat3(1.0f);
	// Task: implement the function for rotation.
	// Step1: convert degree to radians (you can use "glm::radians").
	// Step2: recall the 3 basic rotation matrices (around x, y and z) you learn in class.
	// Step3: there should be 3 cases, conditioned on which axis to rotate around.
	// Step4: fill in the rotation matrix "res" (above).

	double rad = glm::radians(degree);
	double sinvalue = sin(rad);
	double cosvalue = cos(rad);

	glm::vec3 row1 = glm::vec3(0.0f);
	glm::vec3 row2 = glm::vec3(0.0f);
	glm::vec3 row3 = glm::vec3(0.0f);

	if (axis == 1) {
		//X
		row1 = glm::vec3(cosvalue, -sinvalue, 0);
		row2 = glm::vec3(sinvalue, cosvalue, 0);
		row3 = glm::vec3(0, 0, 1);
	}
	else if (axis == 2) {
		//Y
		row1 = glm::vec3(cosvalue, 0, -sinvalue);
		row2 = glm::vec3(0, 1, 0);
		row3 = glm::vec3(sinvalue, 0, cosvalue);
	}
	else if (axis == 3) {
		//Z
		row1 = glm::vec3(1, 0, 0);
		row2 = glm::vec3(0, cosvalue, -sinvalue);
		row3 = glm::vec3(0, sinvalue, cosvalue);
		
	}

	res[0] = row1;
	res[1] = row2;
	res[2] = row3;

	return res;
}

// Generate the geometry corresponding to the string at the given iteration
std::vector<glm::vec3> LSystem::createGeometry(std::string string) {
	std::vector<glm::vec3> verts;
	std::vector<glm::vec3> trunks;
	std::vector<glm::vec3> branches;
	std::vector<glm::vec3> twigs;
	trunk = 0;
	branch = 0;
	twig = 0;

	glm::vec3 cur_pos = glm::vec3(0, 1, 0);
	std::stack<glm::vec3> pos_stack;
	std::stack<glm::vec2> angle_stack;
	double cur_angle_x = 0;
	double cur_angle_y = 0;
	
	for (char c : string) {
		switch (c) {
			case '+':
				cur_angle_y += angle1;
				break;
			case '-':
				cur_angle_y -= angle1;
				break;
			case '*':
				cur_angle_x += angle2;
				break;
			case '^':
				printf("\n");
				cur_angle_x -= angle2;
				break;
			case '[':
				pos_stack.push(cur_pos);
				angle_stack.push(glm::vec2(cur_angle_x, cur_angle_y));
				break;
			case ']':
				cur_pos = pos_stack.top();
				pos_stack.pop();
				cur_angle_x = angle_stack.top()[0];
				cur_angle_y = angle_stack.top()[1];
				angle_stack.pop();
				break;
			case 's':
			case 'S':
				cur_pos.x += sin(glm::radians(cur_angle_y));
				cur_pos.y += cos(glm::radians(cur_angle_y));
				break;
			default:
				if (c == 'G') {
					trunks.push_back(cur_pos);
					trunk++;
				}
				else if (c == 'F') {
					branches.push_back(cur_pos);
					branch++;
				}
				else {
					verts.push_back(cur_pos);
				}
				glm::vec3 temp = normalize(normalize(cur_pos) * rotate(cur_angle_x, 2));
				cur_pos += temp * rotate(cur_angle_y, 1);
				
		/*		cur_pos += glm::vec3(normalize(glm::vec4(cur_pos,0)) * glm::rotate(float(glm::radians(cur_angle_x)), glm::vec3(0.f, 1.f, 0.f)) * 
					glm::rotate(float(glm::radians(cur_angle_y)), glm::vec3(1.f,0.f,0.f)));*/
				/*cur_pos.x += sin(glm::radians(cur_angle_y));
				cur_pos.y += cos(glm::radians(cur_angle_y));
				cur_pos.z = 0;*/
				if (c == 'G') {
					trunks.push_back(cur_pos);
					trunk++;
				}
				else if (c == 'F') {
					branches.push_back(cur_pos);
					branch++;
				}
				else {
					verts.push_back(cur_pos);
				}
		}
		
	}
	for (int i = branch - 1; i >= 0; i--) {
		verts.insert(verts.begin(), branches[i]);
	}
	for (int i = trunk - 1; i >= 0; i--) {
		verts.insert(verts.begin(), trunks[i]);
	}
	

	return verts;
}

// Add given geometry to the OpenGL vertex buffer and update state accordingly
void LSystem::addVerts(std::vector<glm::vec3>& verts) {
	// Add iteration data
	IterData id;
	if (iterData.empty())
		id.first = 0;
	else {
		auto& lastID = iterData.back();
		id.first = lastID.first + lastID.count;
	}
	id.count = verts.size();

	// Calculate bounding box and create adjustment matrix
	glm::vec3 minBB = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 maxBB = glm::vec3(std::numeric_limits<float>::lowest());
	for (auto& v : verts) {
		minBB = glm::min(minBB, v);
		maxBB = glm::max(maxBB, v);
	}
	glm::vec3 diag = maxBB - minBB;
	float scale = 1.9f / glm::max(glm::max(diag.x, diag.y), diag.z);
	id.bbfix = glm::mat4(1.0f);
	id.bbfix[0][0] = scale;
	id.bbfix[1][1] = scale;
	id.bbfix[2][2] = scale;
	id.bbfix[3] = glm::vec4(-(minBB + maxBB) * scale / 2.0f, 1.0f);
	iterData.push_back(id);

	GLsizei newSize = (id.first + id.count) * sizeof(glm::vec3);
	if (newSize > bufSize) {
		// Create a new vertex buffer to hold vertex data
		GLuint tempBuf;
		glGenBuffers(1, &tempBuf);
		glBindBuffer(GL_ARRAY_BUFFER, tempBuf);
		glBufferData(GL_ARRAY_BUFFER,
			newSize, nullptr, GL_STATIC_DRAW);

		// Copy data from existing buffer
		if (vbo) {
			glBindBuffer(GL_COPY_READ_BUFFER, vbo);
			glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_ARRAY_BUFFER, 0, 0, bufSize);
			glBindBuffer(GL_COPY_READ_BUFFER, 0);
			glDeleteBuffers(1, &vbo);
		}

		vbo = tempBuf;
		bufSize = newSize;

	} else
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// Upload new vertex data

	for (auto& v : verts) {
		//printf("x: %f y: %f z: %f\n", v.x, v.y, v.z);
	}

	glBufferSubData(GL_ARRAY_BUFFER,
		id.first * sizeof(glm::vec3), id.count * sizeof(glm::vec3), verts.data());


	// Reset vertex data source (format)
	if (!vao) {
		glGenVertexArrays(1, &vao);
	}
	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (GLvoid*)0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//glEnableVertexAttribArray(1);

	//GLuint vbo1;
	//glGenBuffers(1, &vbo1);
	//glBindBuffer(GL_ARRAY_BUFFER, vbo1);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	//glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(glm::vec3), (void*)0);

}

// Compile and link shader
void LSystem::initShader() {
	std::vector<GLuint> shaders;
	shaders.push_back(compileShader(GL_VERTEX_SHADER, "shaders/v.glsl"));
	shaders.push_back(compileShader(GL_FRAGMENT_SHADER, "shaders/f.glsl"));
	shader = linkProgram(shaders);
	// Cleanup extra state
	for (auto s : shaders)
		glDeleteShader(s);
	shaders.clear();

	// Get uniform locations
	xformLoc = glGetUniformLocation(shader, "xform");
}


// Remove empty lines, comments, and trim leading and trailing whitespace
std::stringstream preprocessStream(std::istream& istr) {
	istr.exceptions(istr.badbit | istr.failbit);
	std::stringstream ss;

	try {
		while (true) {
			std::string line = getNextLine(istr);
			ss << line << std::endl;	// Add newline after each line
		}								// Stream always ends with a newline

	} catch (const std::exception& e) {
		if (!istr.eof()) throw e;
	}

	return ss;
}

// Reads lines from istream, stripping whitespace and comments,
// until it finds a nonempty line
std::string getNextLine(std::istream& istr) {
	const std::string comment = "#";
	std::string line = "";
	while (line == "") {
		std::getline(istr, line);
		// Skip comments and empty lines
		auto found = line.find(comment);
		if (found != std::string::npos)
			line = line.substr(0, found);
		line = trim(line);
	}
	return line;
}

// Trim leading and trailing whitespace from a line
std::string trim(const std::string& line) {
	const std::string whitespace = " \t\r\n";
	auto first = line.find_first_not_of(whitespace);
	if (first == std::string::npos)
		return "";
	auto last = line.find_last_not_of(whitespace);
	auto range = last - first + 1;
	return line.substr(first, range);
}
