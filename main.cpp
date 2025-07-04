#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <ctime>
#include <vector>
#include <iomanip>

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

struct Purchase {
	int id;
	string name;
	double quantity;
	double price;
	string date;
	string type;
};

vector<Purchase> loadPurchases(sqlite3* db) {
	vector<Purchase> purchases;
	string command = "SELECT id, name, quantity, price, date, type FROM purchases ORDER BY date DESC;";
	sqlite3_stmt* stmt;
	
	if (sqlite3_prepare_v2(db, command.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			Purchase p;
			p.id = sqlite3_column_int(stmt, 0);
			p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
			p.quantity = sqlite3_column_double(stmt, 2);
			p.price = sqlite3_column_double(stmt, 3);
			p.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
			p.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
			purchases.push_back(p);
		}
	}
	sqlite3_finalize(stmt);
	
	return purchases;
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
	GLFWwindow* win = glfwCreateWindow(1000, 600, "Buyer Notes", nullptr, nullptr);
	glfwMakeContextCurrent(win);
	glfwSwapInterval(1); // V-sync

	// Init Nuklear GLFW wrapper
	struct nk_glfw glfw = {};
	nk_glfw3_init(&glfw, win, (nk_glfw_init_state) 1);
	struct nk_colorf bg = {0.10f, 0.18f, 0.24f, 1.0f};

	// Load fonts
	struct nk_font_atlas *atlas;
	nk_glfw3_font_stash_begin(&glfw, &atlas);
	struct nk_font *bigFont = nk_font_atlas_add_default(atlas, 18.0f, nullptr);
	nk_glfw3_font_stash_end(&glfw);
	nk_style_set_font(&glfw.ctx, &bigFont->handle);

	// Insert test data
	insert(db, "apple", 5, 0.5);
	insert(db, "apple", 2, 0.6);
	insert(db, "orange", 1, 1.5);

	// Main loop
	while (!glfwWindowShouldClose(win)) {
		// Poll events and start a new frame
		glfwPollEvents();
		nk_glfw3_new_frame(&glfw);

		// Load purchases
		vector<Purchase> purchases = loadPurchases(db);

		// Purchases Window (Left Side)
		if (nk_begin(&glfw.ctx, "Purchases", nk_rect(0, 0, 500, 600), NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE)) {
			float widths[] = {0.2f, 0.15f, 0.15f, 0.15f, 0.35f};
		
			// Header row
			nk_layout_row(&glfw.ctx, NK_DYNAMIC, 15, 5, widths);
			nk_label(&glfw.ctx, "Name", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Type", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Qty", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Price", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Date", NK_TEXT_LEFT);
		
			// Data rows
			for (const Purchase& p : purchases) {
				nk_layout_row(&glfw.ctx, NK_DYNAMIC, 15, 5, widths);

				nk_label(&glfw.ctx, p.name.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, p.type.c_str(), NK_TEXT_LEFT);

				nk_label(&glfw.ctx, (ostringstream() << fixed << setprecision(2) << p.quantity).str().c_str(), NK_TEXT_LEFT);

				string priceStr = to_string(p.price);
				nk_label(&glfw.ctx, (ostringstream() << fixed << setprecision(2) << p.price).str().c_str(), NK_TEXT_LEFT);

				nk_label(&glfw.ctx, p.date.c_str(), NK_TEXT_LEFT);
			}
		}
		nk_end(&glfw.ctx);
		
		// Properties Window (Right Side)
		if (nk_begin(&glfw.ctx, "Properties", nk_rect(500, 0, 500, 600), NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE)) {
			nk_layout_row_dynamic(&glfw.ctx, 25, 1);
			nk_label(&glfw.ctx, "Properties go here.", NK_TEXT_LEFT);
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
