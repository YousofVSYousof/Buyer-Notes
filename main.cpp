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

// Helper functions
#include "helpers.hpp"

using namespace std;

int main() {
	// Database Initialization
	sqlite3 *db;
	int rc = sqlite3_open("data.db", &db);
	if (rc) {
		cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
		return 1;
	}

	runCommand(db, "CREATE TABLE IF NOT EXISTS purchases ("
	                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
	                    "name TEXT,"
	                    "quantity REAL,"
	                    "price REAL,"
	                    "timeStamp TEXT,"
	                    "type TEXT"
	                ");");

	// GLFW Initialization
	glfwInit();
	GLFWwindow* win = glfwCreateWindow(1100, 600, "Buyer Notes", nullptr, nullptr);
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
		if (nk_begin(&glfw.ctx, "Purchases", nk_rect(0, 0, 600, 600), NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE)) {
			float widths[] = {0.2f, 0.15f, 0.1f, 0.15f, 0.2f, 0.2f};
		
			// Header row
			nk_layout_row(&glfw.ctx, NK_DYNAMIC, 15, 6, widths);
			nk_label(&glfw.ctx, "Name", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Type", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Qty", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Price", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Date", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Total", NK_TEXT_LEFT);
			
			// Data rows
			for (size_t i = 0; i < purchases.size(); ++i) {
				const Purchase& p = purchases[i];
		
				nk_layout_row(&glfw.ctx, NK_DYNAMIC, 25, 6, widths);
				
				nk_label(&glfw.ctx, p.name.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, p.type.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, formatDouble(p.quantity).c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, formatDouble(p.price).c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, p.timeStamp.substr(0, 10).c_str(), NK_TEXT_LEFT);
		
				// Show total only if this is the last purchase of that day
				bool lastOfDay = (i == 0) || (p.timeStamp.substr(0, 10) != purchases[i - 1].timeStamp.substr(0, 10));
				
				if (lastOfDay) {
					double total = getTotalSpentOnDate(db, p.timeStamp);
					nk_label(&glfw.ctx, formatDouble(total).c_str(), NK_TEXT_LEFT);
				} else {
					nk_label(&glfw.ctx, "", NK_TEXT_LEFT);
				}
			}
		}
		nk_end(&glfw.ctx);
		
		// Statistics Window (Right Side)
		if (nk_begin(&glfw.ctx, "Statistics", nk_rect(600, 0, 500, 600), NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE)) {
			nk_layout_row_dynamic(&glfw.ctx, 25, 1);
		
			double totalLastMonth = getTotalSpent(db, 30);
			double avgPerDayLastMonth = getAverageSpentPerDayLastMonth(db);
		
			string totalStr = "Total spent last month: $" + formatDouble(totalLastMonth);
			string avgStr = "Average spent per day last month: $" + formatDouble(avgPerDayLastMonth);
		
			nk_label(&glfw.ctx, totalStr.c_str(), NK_TEXT_LEFT);
			nk_label(&glfw.ctx, avgStr.c_str(), NK_TEXT_LEFT);
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
