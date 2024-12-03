#define NANOSVG_IMPLEMENTATION	// Expands implementation
#include "..\3dparty/nanosvg/src/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "..\3dparty/nanosvg/src/nanosvgrast.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#define TINYGLTF_IMPLEMENTATION
#include "..\common\gltf_loader.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "..\common\debugging.h"
#include "..\common\renderable.h"
#include "..\common\shaders.h"
#include "..\common\simple_shapes.h"

#include "..\common\carousel\carousel.h"
#include "..\common\carousel\carousel_to_renderable.h"
#include "..\common\carousel\carousel_loader.h"

#include <iostream>
#include <algorithm>
#include <conio.h>
#include <direct.h>
#include "..\common\matrix_stack.h"
#include "..\common\intersection.h"
#include "..\common\trackball.h"
#include "..\common\texture.h"

#include <..\3dparty\imgui\imgui.h>
#include <..\3dparty\imgui\imgui_impl_glfw.h>
#include <..\3dparty\imgui\imgui_impl_opengl3.h>



trackball tb;

/* projection matrix*/
glm::mat4 proj;

/* view matrix */
glm::mat4 view;

matrix_stack stack;

/* light direction in world space*/
glm::vec4 Ldir;

struct SpotLight {
	glm::vec3 position;
	glm::vec3 direction;
	glm::vec3 color;
	// Attenuazione
	float constant;
	float linear;
	float quadratic;
	// Angoli del cono
	float cutoff;
	float outerCutoff;
};
std::vector<SpotLight> spotLights;

struct TreeInfo {
	glm::vec3 position;  // Posizione dell'albero nella scena
	float height;        // Altezza dell'albero
};
std::vector<TreeInfo> treeInfos;

// Funzione che estrae la posizione e l'altezza di ogni albero e la salva nella struttura TreeInfo
void extract_tree_info(const race& r) {
	const auto& stick_objects = r.trees();

	for (const auto& obj : stick_objects) {
		TreeInfo info;
		info.position = obj.pos;
		info.height = obj.height;
		treeInfos.push_back(info);
	}
}

struct LampInfo {
	glm::vec3 position;  // Posizione del lampione nella scena
	float height;        // Altezza del lampione
};
std::vector<LampInfo> lampInfos;

// Funzione che estrae la posizione e l'altezza di ogni lampione e la salva nella struttura LampInfo
void extract_lamp_info(const race& r) {
	const auto& stick_objects = r.lamps();

	for (const auto& obj : stick_objects) {
		LampInfo info;
		info.position = obj.pos;
		info.height = obj.height;
		lampInfos.push_back(info);
	}
}

// Funzione per generare vertici e coordinate texture del terreno
void generateTerrainMesh(const terrain& terr, std::vector<float>& vertices, std::vector<unsigned int>& indices) {
	int width = terr.size_pix.x;
	int height = terr.size_pix.y;

	float scaleX = terr.rect_xz[2] / (width - 1);
	float scaleZ = terr.rect_xz[3] / (height - 1);

	// Fattore di ripetizione della texture
	float repeatFactor = 1.0f; // Cambia questo valore per ripetere la texture più o meno volte

	// Generazione dei vertici e delle coordinate texture
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			// Calcolo delle coordinate x e z
			float z = terr.rect_xz[0] + j * scaleX;
			float x = terr.rect_xz[1] + i * scaleZ;

			// Assicurati che l'indice sia all'interno dei limiti del height field
			float y = terr.hf(i, j); // Ottieni l'altezza

			// Vertice: posizioni (x, y, z)
			vertices.push_back(x);
			vertices.push_back(y);
			vertices.push_back(z);

			// Coordinate texture (u, v) normalizzate e scalate per la ripetizione
			float u = static_cast<float>(j) / (width - 1) * repeatFactor;
			float v = static_cast<float>(i) / (height - 1) * repeatFactor;
			vertices.push_back(u);
			vertices.push_back(v);
		}
	}

	// Generazione degli indici per i triangoli
	for (int i = 0; i < height - 1; ++i) {
		for (int j = 0; j < width - 1; ++j) {
			int topLeft = i * width + j;
			int topRight = topLeft + 1;
			int bottomLeft = (i + 1) * width + j;
			int bottomRight = bottomLeft + 1;

			// Primo triangolo
			indices.push_back(topLeft);
			indices.push_back(bottomLeft);
			indices.push_back(topRight);

			// Secondo triangolo
			indices.push_back(topRight);
			indices.push_back(bottomLeft);
			indices.push_back(bottomRight);
		}
	}
}

// Funzione per generare vertici e coordinate texture della strada
void generateTrackMesh(const track& trk, std::vector<float>& vertices, std::vector<unsigned int>& indices) {
	float totalLength = trk.length;

	// Dimensioni della texture e fattore di ripetizione
	const float textureWidth = 512.0f; // larghezza in pixel della texture

	//AUMENTA textureRepeatFactor SE LA STRADA SEMBRA TROPPO GRANDE
	float textureRepeatFactor = 30.0f;  // fattore di ripetizione della texture

	// Assicurati che entrambi i bordi abbiano lo stesso numero di punti
	assert(trk.curbs[0].size() == trk.curbs[1].size());

	// Variabile per tracciare la lunghezza accumulata lungo la curva sinistra
	float accumulatedLength = 0.0f;

	// Generazione dei vertici e delle coordinate texture
	for (size_t i = 0; i < trk.curbs[0].size(); ++i) {
		// Punti delle curbs
		glm::vec3 left = trk.curbs[0][i];
		glm::vec3 right = trk.curbs[1][i];

		// Calcolo della distanza tra i punti successivi per la texture mapping
		if (i > 0) {
			accumulatedLength += glm::length(left - trk.curbs[0][i - 1]);
		}

		// Calcolo delle coordinate texture per una rotazione di 90 gradi
		float vCoord = accumulatedLength / textureWidth * textureRepeatFactor;

		// Vertice sinistro
		vertices.push_back(left.x);
		vertices.push_back(left.y);
		vertices.push_back(left.z);
		vertices.push_back(0.0f); // u
		vertices.push_back(vCoord); // v

		// Vertice destro
		vertices.push_back(right.x);
		vertices.push_back(right.y);
		vertices.push_back(right.z);
		vertices.push_back(1.0f); // u
		vertices.push_back(vCoord); // v
	}

	// Generazione degli indici per disegnare la strada con un triangolo strip
	for (size_t i = 0; i < trk.curbs[0].size() - 1; ++i) {
		unsigned int topLeft = i * 2;
		unsigned int bottomLeft = topLeft + 1;
		unsigned int topRight = (i + 1) * 2;
		unsigned int bottomRight = topRight + 1;

		indices.push_back(topLeft);
		indices.push_back(bottomLeft);
		indices.push_back(topRight);

		indices.push_back(topRight);
		indices.push_back(bottomLeft);
		indices.push_back(bottomRight);
	}
}

glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 0.9f);
int dayTime = 1;
int selected = 1;
int selected_camera = 0;
int previous_selected_camera = -1;
// ImGui
void gui_setup() {
	int selected_mesh = 0;
	ImGui::BeginMainMenuBar();
	if (ImGui::BeginMenu("Day Time")) {
		if (ImGui::Selectable("Day", selected_mesh == 0)) dayTime = 1;
		if (ImGui::Selectable("Night", selected_mesh == 1)) dayTime = 0;
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Models Mode")) {
		if (ImGui::Selectable("Color only", selected == 0)) selected = 0;
		if (ImGui::Selectable("Normal Mapping", selected == 1)) selected = 1;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Point of View")) {
		if (ImGui::Selectable("World", selected_camera == 0)) selected_camera = 0;
		if (ImGui::Selectable("Cameraman", selected_camera == 1)) selected_camera = 1;
		ImGui::EndMenu();
	}


	ImGui::EndMainMenuBar();
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	// Esegui solo se la camera selezionata è quella che guarda tutta la scena
	if (selected_camera == 0) {
		tb.mouse_move(proj, view, xpos, ypos);
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// Esegui solo se la camera selezionata è quella che guarda tutta la scena
	if (selected_camera == 0) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
			double xpos, ypos;
			glfwGetCursorPos(window, &xpos, &ypos);
			tb.mouse_press(proj, view, xpos, ypos);
		}
		else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
			tb.mouse_release();
		}
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// Esegui solo se la camera selezionata è quella che guarda tutta la scena
	if (selected_camera == 0) {
		tb.mouse_scroll(xoffset, yoffset);
	}
}

// Funzione di utilità per stampare una matrice 4x4
void PrintMatrix(const glm::mat4& matrix, const std::string& name) {
	std::cout << name << ":\n";
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			std::cout << matrix[i][j] << " ";
		}
		std::cout << "\n";
	}
	std::cout << std::endl;
}

texture grass_tex, grass_normal, normal_map, street_tex;
char grass_tex_name[65536] = { "textures/grass_tile.png" };
char grass_normal_name[65536] = { "textures/grass_normal1.png" };
char normal_map_name[65536] = { "textures/normal_map.jpg" };
char street_tex_name[65536] = { "textures/street_tile.png" };

int main(int argc, char** argv)
{

	race r;

	carousel_loader::load("small_test.svg", "terrain_256.png", r);

	//add 10 cars
	for (int i = 0; i < 10; ++i) {
		r.add_car();
	}

	GLFWwindow* window;

	/* Initialize the library */
	if (!glfwInit())
		return -1;

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(800, 800, "CarOusel", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}
	/* declare the callback functions on mouse events */
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	glewInit();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplOpenGL3_Init();
	ImGui_ImplGlfw_InitForOpenGL(window, true);

	printout_opengl_glsl_info();

	renderable fram = shape_maker::frame();

	renderable r_cube = shape_maker::cube();

	renderable cube = shape_maker::cube();

	renderable r_track;
	r_track.create();
	game_to_renderable::to_track(r, r_track);
	r_track.mater.base_color_texture = street_tex.load(std::string(street_tex_name), 0);
	r_track.mater.normal_texture = normal_map.load(std::string(normal_map_name), 1);
	// Buffer dei vertici e indici della strada
	std::vector<float> track_vertices;
	std::vector<unsigned int> track_indices;
	// Genera i vertici e gli indici della strada
	generateTrackMesh(r.t(), track_vertices, track_indices);
	// Passa le posizioni e le coordinate texture al vertex shader
	r_track.add_vertex_attribute(track_vertices.data(), track_vertices.size(), 0, 3, sizeof(float) * 5, 0); // Posizione
	r_track.add_vertex_attribute(track_vertices.data(), track_vertices.size(), 1, 2, 5 * sizeof(float), 3 * sizeof(float)); // Coordinate texture (u, v)
	r_track.add_indices<unsigned int>(track_indices.data(), track_indices.size(), GL_TRIANGLE_STRIP);

	renderable r_terrain;
	r_terrain.create();
	game_to_renderable::to_heightfield(r, r_terrain);
	r_terrain.mater.base_color_texture = grass_tex.load(std::string(grass_tex_name), 0);
	r_terrain.mater.normal_texture = grass_normal.load(std::string(grass_normal_name), 1);
	// Buffer dei vertici e indici del terreno
	std::vector<float> ter_vertices;
	std::vector<unsigned int> ter_indices;
	// Genera i vertici e gli indici del terreno
	generateTerrainMesh(r.ter(), ter_vertices, ter_indices);
	// Passa le posizioni e le coordinate texture al vertex shader
	r_terrain.add_vertex_attribute(ter_vertices.data(), ter_vertices.size(), 0, 3, sizeof(float) * 5, 0); // Posizione
	r_terrain.add_vertex_attribute(ter_vertices.data(), ter_vertices.size(), 1, 2, sizeof(float) * 5, sizeof(float) * 3); // Coordinate texture
	r_terrain.add_indices<unsigned int>(ter_indices.data(), ter_indices.size(), GL_TRIANGLES);

	renderable r_trees;
	r_trees.create();
	game_to_renderable::to_tree(r, r_trees);
	extract_tree_info(r);

	renderable r_lamps;
	r_lamps.create();
	game_to_renderable::to_lamps(r, r_lamps);
	extract_lamp_info(r);

	tb.reset();
	tb.set_center_radius(glm::vec3(0, 0, 0), 1.f);

	for (const auto& obj : lampInfos) {
		SpotLight light;
		
		//Non inserisco la posizione perché deve essere calcolata dentro il loop di render
		light.direction = glm::vec3(0.0f, -1.0f, 0.0f);  // Direzione della spotlight
		light.color = glm::vec3(0.7f, 0.7f, 0.5f);  // Luce gialla/bianca
		// Controlla l'attenuazione
		light.constant = 1.0f;
		light.linear = 10.0f;//0.54
		light.quadratic = 5.f;//0.192
		// Angoli del cono
		light.cutoff = 12.5f;  // Angolo interno in gradi
		light.outerCutoff = 17.5f;  // Angolo esterno in gradi
		//std::cout << "Light Position: ("<< light.position.x << ", "<< light.position.y << ", "<< light.position.z << ")" << std::endl;
		spotLights.push_back(light);
	}
	
	shader basic_shader, model_shader;
	basic_shader.create_program("shaders/basic.vert", "shaders/basic.frag");
	model_shader.create_program("shaders/model.vert", "shaders/model.frag");

	/* use the program shader "program_shader" */
	glUseProgram(basic_shader.program);
	glUniform1i(basic_shader["TextureSampler"], 0);
	glUniform1i(basic_shader["NormalMapSampler"], 1);
	glUseProgram(0);

	glUseProgram(model_shader.program);
	glUniform1i(model_shader["TextureSampler"], 0);
	glUniform1i(model_shader["NormalMapSampler"], 1);
	glUseProgram(0);

	//Caricamento albero
	gltf_loader tree_loader; // Istanza del loader glTF
	std::string tree_filename = "models/tree.glb";
	std::vector<renderable> tree; // Contenitore per i renderable creati dal loader
	box3 tree_bbox; // Bounding box della scena
	tree_loader.load_to_renderable(tree_filename, tree, tree_bbox);

	//Caricamento auto
	gltf_loader cars_loader;
	std::string car_filename = "models/cherrier_vivace_rally.glb";
	std::vector<renderable> macchina; // Contenitore per i renderable creati dal loader
	box3 car_bbox; // Bounding box della scena
	cars_loader.load_to_renderable(car_filename, macchina, car_bbox);

	//Caricamento lampione
	gltf_loader lamps_loader;
	std::string lamp_filename = "models/spherical_street_lamp.glb";
	std::vector<renderable> lampione; // Contenitore per i renderable creati dal loader
	box3 lamp_bbox; // Bounding box della scena
	lamps_loader.load_to_renderable(lamp_filename, lampione, lamp_bbox);

	//Caricamento cameraman
	gltf_loader cams_loader;
	std::string cam_filename = "models/arri_camera.glb";
	std::vector<renderable> cam; // Contenitore per i renderable creati dal loader
	box3 cam_bbox; // Bounding box della scena
	cams_loader.load_to_renderable(cam_filename, cam, cam_bbox);

	// initial light direction
	Ldir = glm::vec4(0.0, 1.0, 0.0, 0.0);

	// Punto di vista iniziale
	glm::vec4 Vdir = glm::vec4(0.0, 1.0, 2.0, 1.0);

	// define the viewport
	glViewport(0, 0, 800, 800);

	proj = glm::perspective(glm::radians(45.f), 1.f, 0.2f, 10.f);

	view = glm::lookAt(glm::vec3(tb.matrix() * Vdir), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));

	glUseProgram(model_shader.program);
	glUniformMatrix4fv(model_shader["uProj"], 1, GL_FALSE, &proj[0][0]);
	glUniformMatrix4fv(model_shader["uView"], 1, GL_FALSE, &view[0][0]);

	for (int i = 0; i < spotLights.size(); ++i) {
		std::string baseName = "spotLights[" + std::to_string(i) + "]";

		//GLuint posLoc = glGetUniformLocation(model_shader.program, (baseName + ".position").c_str());
		GLuint dirLoc = glGetUniformLocation(model_shader.program, (baseName + ".direction").c_str());
		GLuint colorLoc = glGetUniformLocation(model_shader.program, (baseName + ".color").c_str());
		GLuint constantLoc = glGetUniformLocation(model_shader.program, (baseName + ".constant").c_str());
		GLuint linearLoc = glGetUniformLocation(model_shader.program, (baseName + ".linear").c_str());
		GLuint quadraticLoc = glGetUniformLocation(model_shader.program, (baseName + ".quadratic").c_str());
		GLuint cutoffLoc = glGetUniformLocation(model_shader.program, (baseName + ".cutoff").c_str());
		GLuint outerCutoffLoc = glGetUniformLocation(model_shader.program, (baseName + ".outerCutoff").c_str());

		//glUniform3fv(posLoc, 1, &spotLights[i].position[0]);
		glUniform3fv(dirLoc, 1, &spotLights[i].direction[0]);
		glUniform3fv(colorLoc, 1, &spotLights[i].color[0]);
		glUniform1f(constantLoc, spotLights[i].constant);
		glUniform1f(linearLoc, spotLights[i].linear);
		glUniform1f(quadraticLoc, spotLights[i].quadratic);
		glUniform1f(cutoffLoc, glm::cos(glm::radians(spotLights[i].cutoff)));  // Coseno dell'angolo
		glUniform1f(outerCutoffLoc, glm::cos(glm::radians(spotLights[i].outerCutoff)));  // Coseno dell'angolo esterno
	}
	glUseProgram(0);
	
	glUseProgram(basic_shader.program);
	glUniformMatrix4fv(basic_shader["uProj"], 1, GL_FALSE, &proj[0][0]);
	glUniformMatrix4fv(basic_shader["uView"], 1, GL_FALSE, &view[0][0]);

	for (int i = 0; i < spotLights.size(); ++i) {
		std::string baseName = "spotLights[" + std::to_string(i) + "]";

		GLuint dirLoc = glGetUniformLocation(basic_shader.program, (baseName + ".direction").c_str());
		GLuint colorLoc = glGetUniformLocation(basic_shader.program, (baseName + ".color").c_str());
		GLuint constantLoc = glGetUniformLocation(basic_shader.program, (baseName + ".constant").c_str());
		GLuint linearLoc = glGetUniformLocation(basic_shader.program, (baseName + ".linear").c_str());
		GLuint quadraticLoc = glGetUniformLocation(basic_shader.program, (baseName + ".quadratic").c_str());
		GLuint cutoffLoc = glGetUniformLocation(basic_shader.program, (baseName + ".cutoff").c_str());
		GLuint outerCutoffLoc = glGetUniformLocation(basic_shader.program, (baseName + ".outerCutoff").c_str());

		glUniform3fv(dirLoc, 1, &spotLights[i].direction[0]);
		glUniform3fv(colorLoc, 1, &spotLights[i].color[0]);
		glUniform1f(constantLoc, spotLights[i].constant);
		glUniform1f(linearLoc, spotLights[i].linear);
		glUniform1f(quadraticLoc, spotLights[i].quadratic);
		glUniform1f(cutoffLoc, glm::cos(glm::radians(spotLights[i].cutoff)));  // Coseno dell'angolo
		glUniform1f(outerCutoffLoc, glm::cos(glm::radians(spotLights[i].outerCutoff)));  // Coseno dell'angolo esterno
	}

	r.start(11, 0, 0, 600);
	r.update();

	// Definizione della posizione della luce e della luce ambientale
	glm::vec3 fixedLightDirection = glm::vec3(0.0f, -1.0f, 0.0f); // Luce proveniente dall'alto
	glm::vec3 ambientColor(0.3f, 0.3f, 0.3f);  // Colore grigio chiaro

	// Passa il vettore della direzione della luce al fragment shader
	glUniform3fv(basic_shader["sunlightDirection"], 1, &fixedLightDirection[0]);
	glUseProgram(0);

	matrix_stack stack;

	glm::mat4 camera_frame = r.cameramen()[3].frame;
	
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		/* Render here */
		glClearColor(0.3f, 0.3f, 0.3f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		check_gl_errors(__LINE__, __FILE__);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		gui_setup();

		/* Poll for and process events */
		glfwPollEvents();

		/* light direction transformed by the trackball tb*/
		glm::vec4 curr_Ldir = tb.matrix() * Ldir;

		glUseProgram(model_shader.program);
		glUniform4fv(model_shader["uLdir"], 1, &curr_Ldir[0]);
		glUniform1i(model_shader["uRenderMode"], selected);
		glUniform1i(model_shader["dayTime"], dayTime);
		GLuint lightColorLoc1 = glGetUniformLocation(model_shader.program, "lightColor");
		glUniform3fv(lightColorLoc1, 1, &lightColor[0]);
		glUseProgram(0);

		glUseProgram(basic_shader.program);
		glUniform1i(basic_shader["dayTime"], dayTime);

		// Passa il colore della luce
		GLuint lightColorLoc = glGetUniformLocation(basic_shader.program, "lightColor");
		glUniform3fv(lightColorLoc, 1, &lightColor[0]);

		// Passa la posizione della luce ambientale
		GLuint ambientColorLoc = glGetUniformLocation(basic_shader.program, "ambientColor");
		glUniform3fv(ambientColorLoc, 1, &ambientColor[0]);

		r.update();
		stack.load_identity();
		stack.push();
		stack.mult(tb.matrix());
		glUniformMatrix4fv(basic_shader["uModel"], 1, GL_FALSE, &stack.m()[0][0]);

		fram.bind();
		glDrawArrays(GL_LINES, 0, 6);

		glColor3f(0, 0, 1);
		glBegin(GL_LINES);
		glVertex3f(0, 0, 0);
		glEnd();

		float s = 1.f / r.bbox().diagonal();
		glm::vec3 c = r.bbox().center();

		stack.mult(glm::scale(glm::mat4(1.f), glm::vec3(s)));
		stack.mult(glm::translate(glm::mat4(1.f), -c));

		if (selected_camera != 0 && selected_camera != previous_selected_camera) {
			stack.push();
			stack.mult(camera_frame);
			stack.mult(glm::translate(glm::mat4(1.f), glm::vec3(0, 1.5, 0)));
			stack.mult(glm::scale(glm::mat4(1.f), glm::vec3(0.5, 0.5, 0.5)));
			stack.mult(glm::translate(glm::mat4(1.f), glm::vec3(0.5, 1.5, 0.5)));
			glm::mat4 cam_frame = stack.m() * camera_frame;
			glm::vec3 camera_position = glm::vec3(cam_frame[3]);  // Posizione della telecamera

			// Aggiorna il frame della telecamera per puntare verso la macchina
			view = glm::lookAt(camera_position, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.0f, 1.0f, 0.0f));
			
			std::cout << "Camera Position: (" << camera_position.x << ", " << camera_position.y << ", " << camera_position.z << ")" << std::endl;

			// Aggiorno la variabile che memorizza la camera precedente
			previous_selected_camera = selected_camera;
			stack.pop();

		}
		else if (selected_camera == 0 && selected_camera != previous_selected_camera)
		{
			tb.reset();
			view = glm::lookAt(glm::vec3(tb.matrix() * Vdir), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));

			// Aggiorno la variabile che memorizza la camera precedente
			previous_selected_camera = selected_camera;
		}
		
		glUseProgram(model_shader.program);
		glUniformMatrix4fv(model_shader["uView"], 1, GL_FALSE, &view[0][0]);
		glUseProgram(0);

		glUseProgram(basic_shader.program);
		glUniformMatrix4fv(basic_shader["uView"], 1, GL_FALSE, &view[0][0]);
		
		// Passa la posizione della vista
		glm::vec3 viewPos = glm::vec3(view[3]);
		GLuint viewPosLoc = glGetUniformLocation(basic_shader.program, "viewPos");
		glUniform3fv(viewPosLoc, 1, &viewPos[0]);

		//	TERRENO
		glDepthRange(0.01, 1);
		glUniformMatrix4fv(basic_shader["uModel"], 1, GL_FALSE, &stack.m()[0][0]);
		r_terrain.bind();
		// Bind della texture e Disegno del terreno
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, r_terrain.mater.base_color_texture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, r_terrain.mater.normal_texture);
		glBindVertexArray(r_terrain.vao);
		glDrawElements(GL_TRIANGLES, 3 * r_terrain.vn, GL_UNSIGNED_INT, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDepthRange(0.0, 1);

		glUseProgram(0);
		glUseProgram(model_shader.program);
		//	MACCHINE
		stack.push();
		for (unsigned int ic = 0; ic < r.cars().size(); ++ic) {
			stack.push();
			fram.bind();
			stack.mult(r.cars()[ic].frame);
			stack.mult(glm::translate(glm::mat4(1.f), glm::vec3(0, 0.25, 0)));
			stack.mult(glm::rotate(glm::mat4(1.f), glm::radians(180.0f), glm::vec3(0, 1, 0)));
			// render each renderable
			for (unsigned int i = 0; i < macchina.size(); ++i) {
				macchina[i].bind();
				stack.push();

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, macchina[i].mater.base_color_texture);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, macchina[i].mater.normal_texture);
								
				glUniformMatrix4fv(model_shader["uModel"], 1, GL_FALSE, &stack.m()[0][0]);
				glDrawElements(macchina[i]().mode, macchina[i]().count, macchina[i]().itype, 0);
				glBindTexture(GL_TEXTURE_2D, 0);
				stack.pop();
			}
			stack.pop();
		}
		stack.pop();

		//	CAMERAMAN
		stack.push();
		fram.bind();
		for (unsigned int ic = 0; ic < r.cameramen().size(); ++ic) {
			stack.push();

			stack.mult(r.cameramen()[ic].frame);
			stack.mult(glm::translate(glm::mat4(1.f), glm::vec3(0, 1.5, 0)));
			stack.mult(glm::rotate(glm::mat4(1.f), glm::radians(90.0f), glm::vec3(1, 0, 0)));
			stack.mult(glm::scale(glm::mat4(1.f), glm::vec3(0.5, 0.5, 0.5)));
			// render each renderable
			for (unsigned int i = 0; i < cam.size(); ++i) {
				cam[i].bind();
				stack.push();

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, cam[i].mater.base_color_texture);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, cam[i].mater.normal_texture);

				glUniformMatrix4fv(model_shader["uModel"], 1, GL_FALSE, &stack.m()[0][0]);
				glDrawElements(cam[i]().mode, cam[i]().count, cam[i]().itype, 0);
				glBindTexture(GL_TEXTURE_2D, 0);
				stack.pop();
			}
			stack.pop();
		}
		stack.pop();

		glUseProgram(0);
		glUseProgram(basic_shader.program);

		//	STRADA
		glUniformMatrix4fv(basic_shader["uModel"], 1, GL_FALSE, &stack.m()[0][0]);
		r_track.bind();
		// Bind della texture e Disegno della strada
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, r_track.mater.base_color_texture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, r_track.mater.normal_texture);
		glBindVertexArray(r_track.vao);
		glDrawElements(GL_TRIANGLE_STRIP, 1.2 * r_track.vn, GL_UNSIGNED_INT, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glUseProgram(0);
		glUseProgram(model_shader.program);

		//	ALBERI
		stack.push();
		for (const auto& obj : treeInfos) {

			glm::vec3 trasl = obj.position;
			float scale = obj.height / 22;

			stack.push();
			stack.mult(glm::translate(glm::mat4(1.f), trasl));
			stack.mult(glm::scale(glm::mat4(1.f), glm::vec3(scale, scale, scale)));

			// render each renderable
			for (unsigned int i = 0; i < tree.size(); ++i) {
				tree[i].bind();
				stack.push();

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, tree[i].mater.base_color_texture);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, tree[i].mater.normal_texture);

				glUniformMatrix4fv(model_shader["uModel"], 1, GL_FALSE, &stack.m()[0][0]);
				glDrawElements(tree[i]().mode, tree[i]().count, tree[i]().itype, 0);
				glBindTexture(GL_TEXTURE_2D, 0);
				stack.pop();
			}

			stack.pop();
		}
		stack.pop();

		//	LAMPIONI
		stack.push();
		for (unsigned j = 0; j < lampInfos.size(); ++j) {
			auto obj = lampInfos[j];
			glm::vec3 trasl = obj.position;
			float scale = obj.height * 1.5;

			stack.push();
			stack.mult(glm::translate(glm::mat4(1.f), trasl));
			stack.mult(glm::translate(glm::mat4(1.f), glm::vec3(0.0, scale * 1.9, 0.0)));
			stack.mult(glm::scale(glm::mat4(1.f), glm::vec3(scale * 1.2, scale, scale * 1.2)));
			stack.mult(glm::rotate(glm::mat4(1.f), glm::radians(90.0f), glm::vec3(1, 0, 0)));

			glm::mat4 final_frame = stack.m();
			glm::vec3 final_pos = glm::vec3(final_frame[3]);
			final_pos.x = final_pos.x + 0.02;
			final_pos.y = final_pos.y + 0.15;
			final_pos.z = final_pos.z + 0.13;
			spotLights[j].position = final_pos;

			std::string baseName = "spotLights[" + std::to_string(j) + "]";
			GLuint posLoc = glGetUniformLocation(model_shader.program, (baseName + ".position").c_str());
			glUniform3fv(posLoc, 1, &spotLights[j].position[0]);

			glUseProgram(0);
			glUseProgram(basic_shader.program);
			GLuint posLoc1 = glGetUniformLocation(basic_shader.program, (baseName + ".position").c_str());
			glUniform3fv(posLoc1, 1, &spotLights[j].position[0]);
			glUseProgram(0);

			glUseProgram(model_shader.program);
			//std::cout << "Light Position: (" << spotLights[j].position.x << ", " << spotLights[j].position.y << ", " << spotLights[j].position.z << ")" << std::endl;

			// render each renderable
			for (unsigned int i = 0; i < lampione.size(); ++i) {
				lampione[i].bind();
				stack.push();

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, lampione[i].mater.base_color_texture);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, lampione[i].mater.normal_texture);

				glUniformMatrix4fv(model_shader["uModel"], 1, GL_FALSE, &stack.m()[0][0]);
				glDrawElements(lampione[i]().mode, lampione[i]().count, lampione[i]().itype, 0);
				glBindTexture(GL_TEXTURE_2D, 0);
				stack.pop();
			}

			stack.pop();
		}
		stack.pop();

		glUseProgram(0);
		stack.pop();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Swap front and back buffers
		glfwSwapBuffers(window);
	}
	glUseProgram(0);
	glfwTerminate();
	return 0;
}
