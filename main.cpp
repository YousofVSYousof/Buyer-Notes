#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <ctime>

// Nuklear / GLFW
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "nuklear.h"

#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
#define NK_GLFW_GL3_IMPLEMENTATION
#include "nuklear_glfw_gl3.h"

using namespace std;

void runCommand(sqlite3 *db, const char* inp) {
	char *error = nullptr;
	int rc = sqlite3_exec(db, inp, nullptr, nullptr, &error);
	if (rc) {
		cerr << "SQL error: " << error << endl;
		exit(1);
	}
}

void runCommand(sqlite3 *db, string inp) {
	runCommand(db, inp.c_str());
}

string getDate() {
    time_t now = time(nullptr);
    tm *ltm = localtime(&now);
    char buffer[11];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", ltm);
    return string(buffer);
}

void insert(sqlite3 *db, string name, double quantity, double price, string type = "") {
	// If type is empty, look for existing type
	if (type.empty()) {
		stringstream query;
		query << "SELECT type FROM purchases WHERE name = '" << name << "' LIMIT 1;";

		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, query.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
			if (sqlite3_step(stmt) == SQLITE_ROW) {
				const unsigned char *foundType = sqlite3_column_text(stmt, 0);
				if (foundType) {
					type = reinterpret_cast<const char*> (foundType);
				}
			}
		}
		sqlite3_finalize(stmt);

		// If still empty, default to "food"
		if (type.empty()) {
			type = "food";
		}
	}

	// Now insert with proper type
	stringstream command;
	command << "INSERT INTO purchases (name, quantity, price, date, type) VALUES ('"
			<< name << "', "
			<< quantity << ", "
			<< price << ", '"
			<< getDate() << "', '"
			<< type << "');";
	cout << command.str() << endl;
	runCommand(db, command.str());
}

double getTotalSpent(sqlite3 *db, string item, int days) {
	stringstream command;
	command << "SELECT SUM(quantity * price) FROM purchases "
			<< "WHERE name = '" << item << "' "
			<< "AND date >= date('now', '-" << days << " day');";

	sqlite3_stmt *stmt;
	double totalSpent = 0;

	sqlite3_prepare_v2(db, command.str().c_str(), -1, &stmt, nullptr);
	sqlite3_step(stmt);

	if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
		totalSpent = sqlite3_column_double(stmt, 0);
	}

	sqlite3_finalize(stmt);
	return totalSpent;
}

double getTotalQuantity(sqlite3 *db, string item, int days) {
	stringstream command;
	command << "SELECT SUM(quantity) FROM purchases "
			<< "WHERE name = '" << item << "' "
			<< "AND date >= date('now', '-" << days << " day');";

	sqlite3_stmt *stmt;
	double totalQuantity = 0;

	sqlite3_prepare_v2(db, command.str().c_str(), -1, &stmt, nullptr);
	sqlite3_step(stmt);

	if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
		totalQuantity = sqlite3_column_double(stmt, 0);
	}

	sqlite3_finalize(stmt);
	return totalQuantity;
}

double getAveragePrice(sqlite3 *db, string item, int days) {
	double totalSpent = getTotalSpent(db, item, days);
	double totalQuantity = getTotalQuantity(db, item, days);

	if (totalQuantity == 0) {
		return 0;
	}

	return totalSpent / totalQuantity;
}

int main() {
	// Database Initialization
	sqlite3 *db;
	int rc = sqlite3_open("test.db", &db);
	if (rc) {
		cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
		return 1;
	}

	runCommand(db, "CREATE TABLE IF NOT EXISTS purchases ("
	                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
	                    "name TEXT,"
	                    "quantity REAL,"
	                    "price REAL,"
	                    "date TEXT,"
	                    "type TEXT"
	                ");");

	// GLFW Initialization
	glfwInit();
	GLFWwindow* win = glfwCreateWindow(800, 600, "Nuklear Example", nullptr, nullptr);
	glfwMakeContextCurrent(win);
	glfwSwapInterval(1); // V-sync

	// Init Nuklear GLFW wrapper
	struct nk_glfw glfw = {};
	nk_glfw3_init(&glfw, win, (nk_glfw_init_state) 1);
	struct nk_colorf bg = {0.10f, 0.18f, 0.24f, 1.0f};

	// Load fonts
	struct nk_font_atlas *atlas;
	nk_glfw3_font_stash_begin(&glfw, &atlas);
	nk_glfw3_font_stash_end(&glfw);

	// Main loop
	while (!glfwWindowShouldClose(win)) {
		glfwPollEvents();
		nk_glfw3_new_frame(&glfw);

		if (nk_begin(&glfw.ctx, "Simple Window", nk_rect(50, 50, 230, 150),
			NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
			NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {

			nk_layout_row_static(&glfw.ctx, 30, 200, 1);
			nk_label(&glfw.ctx, "Hello, Nuklear!", NK_TEXT_ALIGN_CENTERED);
		}
		nk_end(&glfw.ctx);

		// Render
		int width, height;
		glfwGetFramebufferSize(win, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(bg.r, bg.g, bg.b, bg.a);
		glClear(GL_COLOR_BUFFER_BIT);
		nk_glfw3_render(&glfw, NK_ANTI_ALIASING_ON, 512 * 1024, 128 * 1024);
		glfwSwapBuffers(win);
	}

	nk_glfw3_shutdown(&glfw);
	glfwTerminate();
	return 0;
}
