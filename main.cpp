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
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
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

	// Style window headers
	glfw.ctx.style.window.header.label_normal = nk_rgba(120, 255, 255, 255);
	glfw.ctx.style.window.header.label_active = nk_rgba(120, 255, 255, 255);
	glfw.ctx.style.window.header.normal = nk_style_item_color(nk_rgba(0, 0, 0, 255));
	glfw.ctx.style.window.header.active = nk_style_item_color(nk_rgba(20, 20, 20, 255));

	// Global variables
	int selectedIndex = -1;

	// Main loop
	while (!glfwWindowShouldClose(win)) {
		// Poll events and start a new frame
		glfwPollEvents();
		nk_glfw3_new_frame(&glfw);

		// Load purchases
		vector<Purchase> purchases = loadPurchases(db);

		// Purchases Window (Left Side)
		if (nk_begin(&glfw.ctx, "Purchases", nk_rect(0, 0, 600, 600), NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
			float widths[] = {0.2f, 0.15f, 0.1f, 0.15f, 0.2f, 0.2f};
		
			// Header row
			nk_layout_row(&glfw.ctx, NK_DYNAMIC, 15, 6, widths);
			nk_style_push_color(&glfw.ctx, &glfw.ctx.style.text.color, nk_rgba(200, 200, 100, 255));
			nk_label(&glfw.ctx, "Name", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Type", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Qty", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Price", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Date", NK_TEXT_LEFT);
			nk_label(&glfw.ctx, "Total", NK_TEXT_LEFT);
			nk_style_pop_color(&glfw.ctx);
			
			// Data rows
			for (size_t i = 0; i < purchases.size(); ++i) {
				nk_layout_row(&glfw.ctx, NK_DYNAMIC, 25, 6, widths);

				struct nk_command_buffer* buf = nk_window_get_canvas(&glfw.ctx);
				struct nk_rect cellBounds = nk_widget_bounds(&glfw.ctx);
				struct nk_rect rowBounds = nk_rect(cellBounds.x, cellBounds.y, 600, cellBounds.h);
				if (nk_input_is_mouse_hovering_rect(&glfw.ctx.input, rowBounds)) {
					nk_fill_rect(buf, rowBounds, 0, nk_rgba(60, 60, 60, 255));

					if (nk_input_is_mouse_click_in_rect(&glfw.ctx.input, NK_BUTTON_LEFT, rowBounds)) {
						if (selectedIndex != (int) i) {
							selectedIndex = i;
						} else {
							selectedIndex = -1;
						}
					}
				}

				if (selectedIndex == (int) i) {
					nk_fill_rect(buf, rowBounds, 0, nk_rgba(120, 120, 120, 255));
				}

				const Purchase p = purchases[i];
				nk_label(&glfw.ctx, p.name.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, p.type.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, formatDouble(p.quantity).c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, formatDouble(p.price * p.quantity).c_str(), NK_TEXT_LEFT);
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
		if (nk_begin(&glfw.ctx, "Statistics", nk_rect(600, 0, 500, 400), NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
			nk_layout_row_dynamic(&glfw.ctx, 25, 1);
		
			double totalLastMonth = getTotalSpent(db, 30);
			double avgPerDayLastMonth = getAverageSpentPerDayLastMonth(db);

			string totalStr          = "Total left                      : " + formatDouble(-getTotalSpent(db)) + " EGP";
			string totalLastMonthStr = "Total spent last month          : " + formatDouble(totalLastMonth) + " EGP";
			string avgStr            = "Average spent per day last month: " + formatDouble(avgPerDayLastMonth) + " EGP";

			nk_label(&glfw.ctx, totalStr.c_str(), NK_TEXT_LEFT);
			nk_label(&glfw.ctx, totalLastMonthStr.c_str(), NK_TEXT_LEFT);
			nk_label(&glfw.ctx, avgStr.c_str(), NK_TEXT_LEFT);

			// Show item-specific stats if an item is selected
			if (selectedIndex >= 0 && selectedIndex < (int) purchases.size()) {
				const Purchase selected = purchases[selectedIndex];
		
				nk_label(&glfw.ctx, "", NK_TEXT_LEFT);
				nk_style_push_color(&glfw.ctx, &glfw.ctx.style.text.color, nk_rgba(200, 200, 100, 255));
				nk_label(&glfw.ctx, "Selected Item Stats:", NK_TEXT_LEFT);
				nk_style_pop_color(&glfw.ctx);
		
				double totalSpentItem = getTotalSpent(db, selected.name, 30);
				double totalQtyItem = getTotalQuantity(db, selected.name, 30);
				double avgPriceItem = getAveragePrice(db, selected.name, 30);
		
				string itemStr     = "Item                                : " + selected.name;
				string spentStr    = "Total spent on item   (last 30 days): " + formatDouble(totalSpentItem) + " EGP";
				string qtyStr      = "Total quantity bought (last 30 days): " + formatDouble(totalQtyItem);
				string avgPriceStr = "Average price         (last 30 days): " + formatDouble(avgPriceItem) + " EGP";
		
				nk_label(&glfw.ctx, itemStr.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, spentStr.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, qtyStr.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, avgPriceStr.c_str(), NK_TEXT_LEFT);

				// Delete Button
				nk_style_push_style_item(&glfw.ctx, &glfw.ctx.style.button.normal, nk_style_item_color(nk_rgba(200, 50, 50, 255)));
				nk_style_push_style_item(&glfw.ctx, &glfw.ctx.style.button.hover, nk_style_item_color(nk_rgba(255, 80, 80, 255)));
				nk_style_push_style_item(&glfw.ctx, &glfw.ctx.style.button.active, nk_style_item_color(nk_rgba(180, 30, 30, 255)));

				if (nk_button_label(&glfw.ctx, "Delete Selected")) {
					deletePurchase(db, purchases[selectedIndex].id);
					selectedIndex = -1;
				}

				nk_style_pop_style_item(&glfw.ctx);
				nk_style_pop_style_item(&glfw.ctx);
				nk_style_pop_style_item(&glfw.ctx);
			}
		}
		nk_end(&glfw.ctx);

		// Add New Item Window - Bottom Right
		static char nameInput[64] = "";
		static char typeInput[64] = "";
		static char quantityInput[32] = "";
		static char priceInput[32] = "";
		
		if (nk_begin(&glfw.ctx, "Add New Item", nk_rect(600, 400, 500, 600), NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
			static char nameInput[64] = "";
			static char typeInput[64] = "";
			static char quantityInput[32] = "";
			static char priceInput[32] = "";
		
			// Name Row
			nk_layout_row_dynamic(&glfw.ctx, 25, 2);
			nk_label(&glfw.ctx, "Name:", NK_TEXT_LEFT);
			nk_edit_string_zero_terminated(&glfw.ctx, NK_EDIT_FIELD, nameInput, sizeof(nameInput), nk_filter_default);
		
			// Type Row
			nk_layout_row_dynamic(&glfw.ctx, 25, 2);
			nk_label(&glfw.ctx, "Type:", NK_TEXT_LEFT);
			nk_edit_string_zero_terminated(&glfw.ctx, NK_EDIT_FIELD, typeInput, sizeof(typeInput), nk_filter_default);
		
			// Quantity Row
			nk_layout_row_dynamic(&glfw.ctx, 25, 2);
			nk_label(&glfw.ctx, "Quantity:", NK_TEXT_LEFT);
			nk_edit_string_zero_terminated(&glfw.ctx, NK_EDIT_FIELD, quantityInput, sizeof(quantityInput), nk_filter_float);
		
			// Price Row
			nk_layout_row_dynamic(&glfw.ctx, 25, 2);
			nk_label(&glfw.ctx, "Price:", NK_TEXT_LEFT);
			nk_edit_string_zero_terminated(&glfw.ctx, NK_EDIT_FIELD, priceInput, sizeof(priceInput), nk_filter_float);
		
			// Add Button
			nk_layout_row_dynamic(&glfw.ctx, 30, 1);

			nk_style_push_style_item(&glfw.ctx, &glfw.ctx.style.button.normal, nk_style_item_color(nk_rgba(50, 130, 200, 255)));
			nk_style_push_style_item(&glfw.ctx, &glfw.ctx.style.button.hover, nk_style_item_color(nk_rgba(70, 150, 220, 255)));
			nk_style_push_style_item(&glfw.ctx, &glfw.ctx.style.button.active, nk_style_item_color(nk_rgba(40, 110, 180, 255)));

			if (nk_button_label(&glfw.ctx, "Add Item")) {
				double quantity = atof(quantityInput);
				double price = atof(priceInput) / quantity;
		
				if (strlen(nameInput) > 0 && quantity > 0 && strlen(priceInput) > 0) {
					insert(db, nameInput, quantity, price, typeInput);
		
					// Clear inputs
					nameInput[0] = '\0';
					typeInput[0] = '\0';
					quantityInput[0] = '\0';
					priceInput[0] = '\0';

					// Set selection
					selectedIndex = purchases.size();
				}
			}
			
			nk_style_pop_style_item(&glfw.ctx);
			nk_style_pop_style_item(&glfw.ctx);
			nk_style_pop_style_item(&glfw.ctx);
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
