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
	GLFWwindow* win = glfwCreateWindow(1300, 700, "Buyer Notes", nullptr, nullptr);
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
	enum class SelectionType { None, Name, Type, Date, Row } selectionType = SelectionType::None;
	string selectedValue = "";
	int focusedField = 0; // 0 = Name, 1 = Type, 2 = Quantity, 3 = Price

	// Main loop
	while (!glfwWindowShouldClose(win)) {
		// Poll events and start a new frame
		glfwPollEvents();
		nk_glfw3_new_frame(&glfw);

		// Load purchases
		vector<Purchase> purchases = loadPurchases(db);

		// Purchases Window (Left Side)
		if (nk_begin(&glfw.ctx, "Purchases", nk_rect(0, 0, 750, 700), NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
			float widths[] = {0.25f, 0.2f, 0.1f, 0.15f, 0.15f, 0.15f};
		
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
				const Purchase& p = purchases[i];
			
				// Full row bounds
				struct nk_rect cellBounds = nk_widget_bounds(&glfw.ctx);
				struct nk_rect rowBounds = nk_rect(cellBounds.x, cellBounds.y, 750, cellBounds.h);
			
				// Determine if row is highlighted
				bool highlightRow = 
					(selectionType == SelectionType::Name && p.name == selectedValue) ||
					(selectionType == SelectionType::Type && p.type == selectedValue) ||
					(selectionType == SelectionType::Date && p.timeStamp.substr(0, 10) == selectedValue) ||
					(selectionType == SelectionType::Row && selectedIndex == (int) i);
			
				// Hover highlight
				if (nk_input_is_mouse_hovering_rect(&glfw.ctx.input, rowBounds)) {
					nk_fill_rect(buf, rowBounds, 0, nk_rgba(60, 60, 60, 255));
				}
			
				// Selection highlight
				if (highlightRow) {
					nk_fill_rect(buf, rowBounds, 0, nk_rgba(120, 120, 120, 255));
				}
			
				// ---- NAME CELL ----
				struct nk_rect nameBounds = nk_widget_bounds(&glfw.ctx);
				if (nk_input_is_mouse_click_in_rect(&glfw.ctx.input, NK_BUTTON_RIGHT, nameBounds)) {
					selectionType = SelectionType::Name;
					selectedValue = p.name;
					selectedIndex = -1;
				}
				nk_label(&glfw.ctx, p.name.c_str(), NK_TEXT_LEFT);
			
				// ---- TYPE CELL ----
				struct nk_rect typeBounds = nk_widget_bounds(&glfw.ctx);
				if (nk_input_is_mouse_click_in_rect(&glfw.ctx.input, NK_BUTTON_RIGHT, typeBounds)) {
					selectionType = SelectionType::Type;
					selectedValue = p.type;
					selectedIndex = -1;
				}
				nk_label(&glfw.ctx, p.type.c_str(), NK_TEXT_LEFT);
			
				// ---- QUANTITY CELL ----
				string qtyStr = formatDouble(p.quantity);
				nk_label(&glfw.ctx, qtyStr.c_str(), NK_TEXT_LEFT);
			
				// ---- PRICE CELL ----
				string priceStr = formatDouble(p.price * p.quantity);
				nk_label(&glfw.ctx, priceStr.c_str(), NK_TEXT_LEFT);
			
				// ---- DATE CELL ----
				struct nk_rect dateBounds = nk_widget_bounds(&glfw.ctx);
				string dateStr = p.timeStamp.substr(0, 10);
				if (nk_input_is_mouse_click_in_rect(&glfw.ctx.input, NK_BUTTON_RIGHT, dateBounds)) {
					selectionType = SelectionType::Date;
					selectedValue = dateStr;
					selectedIndex = -1;
				}
				nk_label(&glfw.ctx, dateStr.c_str(), NK_TEXT_LEFT);
			
				// ---- TOTAL CELL ----
				bool lastOfDay = (i == 0) || (dateStr != purchases[i - 1].timeStamp.substr(0, 10));
				if (lastOfDay) {
					nk_label(&glfw.ctx, formatDouble(getTotalSpentOnDate(db, p.timeStamp)).c_str(), NK_TEXT_LEFT);
				} else {
					nk_label(&glfw.ctx, "", NK_TEXT_LEFT);
				}
			
				// LEFT CLICK on row selects individual row
				if (nk_input_is_mouse_click_in_rect(&glfw.ctx.input, NK_BUTTON_LEFT, rowBounds)) {
					selectionType = SelectionType::Row;
					selectedIndex = i;
					selectedValue = "";
				}
			}
		}
		nk_end(&glfw.ctx);
		
		// Statistics Window (Right Side)
		if (nk_begin(&glfw.ctx, "Statistics", nk_rect(750, 0, 550, 400), NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
			nk_layout_row_dynamic(&glfw.ctx, 25, 1);
		
			double totalLastMonth = getTotalSpent(db, 30);
			double avgPerDayLastMonth = getAverageSpentPerDayLastMonth(db);

			string totalStr          = "Total left                          : " + formatDouble(-getTotalSpent(db)) + " EGP";
			string totalLastMonthStr = "Total spent last month              : " + formatDouble(totalLastMonth) + " EGP";
			string avgStr            = "Average spent per day last month    : " + formatDouble(avgPerDayLastMonth) + " EGP";

			nk_label(&glfw.ctx, totalStr.c_str(), NK_TEXT_LEFT);
			nk_label(&glfw.ctx, totalLastMonthStr.c_str(), NK_TEXT_LEFT);
			nk_label(&glfw.ctx, avgStr.c_str(), NK_TEXT_LEFT);

			// Show purchase-specific stats only if Row mode is active and a row is selected
			if (selectionType == SelectionType::Row && selectedIndex >= 0 && selectedIndex < (int) purchases.size()) {
				const Purchase& selected = purchases[selectedIndex];
			
				nk_label(&glfw.ctx, "", NK_TEXT_LEFT);
				nk_style_push_color(&glfw.ctx, &glfw.ctx.style.text.color, nk_rgba(200, 200, 100, 255));
				nk_label(&glfw.ctx, "Selected Purchase Stats:", NK_TEXT_LEFT);
				nk_style_pop_color(&glfw.ctx);
			
				string itemStr  = "Name       : " + selected.name;
				string typeStr  = "Type       : " + selected.type;
				string qtyStr   = "Quantity   : " + formatDouble(selected.quantity);
				string priceStr = "Unit Price : " + formatDouble(selected.price) + " EGP";
				string totalStr = "Total Cost : " + formatDouble(selected.price * selected.quantity) + " EGP";
				string dateStr  = "Date       : " + selected.timeStamp.substr(0, 10);
			
				nk_label(&glfw.ctx, itemStr.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, typeStr.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, qtyStr.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, priceStr.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, totalStr.c_str(), NK_TEXT_LEFT);
				nk_label(&glfw.ctx, dateStr.c_str(), NK_TEXT_LEFT);
			
				// Delete Button
				nk_style_push_style_item(&glfw.ctx, &glfw.ctx.style.button.normal, nk_style_item_color(nk_rgba(200, 50, 50, 255)));
				nk_style_push_style_item(&glfw.ctx, &glfw.ctx.style.button.hover, nk_style_item_color(nk_rgba(255, 80, 80, 255)));
				nk_style_push_style_item(&glfw.ctx, &glfw.ctx.style.button.active, nk_style_item_color(nk_rgba(180, 30, 30, 255)));
			
				if (nk_button_label(&glfw.ctx, "Delete Selected")) {
					deletePurchase(db, selected.id);
					selectedIndex = -1;
					selectionType = SelectionType::None;
					selectedValue = "";
				}
			
				nk_style_pop_style_item(&glfw.ctx);
				nk_style_pop_style_item(&glfw.ctx);
				nk_style_pop_style_item(&glfw.ctx);
			}

			// Group stats for Name, Type, Date
			else if (selectionType != SelectionType::None) {
				nk_label(&glfw.ctx, "", NK_TEXT_LEFT);
				nk_style_push_color(&glfw.ctx, &glfw.ctx.style.text.color, nk_rgba(200, 200, 100, 255));
				nk_label(&glfw.ctx, "Selected Group Stats:", NK_TEXT_LEFT);
				nk_style_pop_color(&glfw.ctx);
			
				double totalSpent = 0;
				double totalQty = 0;
				double avgPrice = 0;
				int uniqueItems = 0;
				string traitStr;
			
				if (selectionType == SelectionType::Name) {
					totalSpent = getTotalSpent(db, selectedValue, 30);
					totalQty = getTotalQuantity(db, selectedValue, 30);
					avgPrice = getAveragePrice(db, selectedValue, 30);

					traitStr         = "Name                                : " + selectedValue;
					nk_label(&glfw.ctx, traitStr.c_str(), NK_TEXT_LEFT);
				
					string qtyStr    = "Total quantity bought (last 30 days): " + formatDouble(totalQty);
					nk_label(&glfw.ctx, qtyStr.c_str(), NK_TEXT_LEFT);
					
					string avgStr    = "Average unit price (last 30 days)   : " + formatDouble(avgPrice) + " EGP";
					nk_label(&glfw.ctx, avgStr.c_str(), NK_TEXT_LEFT);
				
				} else if (selectionType == SelectionType::Type) {
					totalSpent = getTotalSpentByType(db, selectedValue, 30);
					int uniqueItems = getUniqueItemsByTypeLast30Days(db, selectedValue);

					traitStr         = "Type                                : " + selectedValue;
					nk_label(&glfw.ctx, traitStr.c_str(), NK_TEXT_LEFT);
				
					string uniqueStr = "Unique items bought (last 30 days)  : " + to_string(uniqueItems);
					nk_label(&glfw.ctx, uniqueStr.c_str(), NK_TEXT_LEFT);
				
				} else if (selectionType == SelectionType::Date) {
					totalSpent = getTotalSpentOnExactDate(db, selectedValue);
					uniqueItems = getUniqueItemsOnDate(db, selectedValue);

					traitStr         = "Date                                : " + selectedValue;
					nk_label(&glfw.ctx, traitStr.c_str(), NK_TEXT_LEFT);
				
					string uniqueStr = "Unique items bought                 : " + to_string(uniqueItems);
					nk_label(&glfw.ctx, uniqueStr.c_str(), NK_TEXT_LEFT);
				}
			
				string spentStr      = "Total spent (last 30 days)          : " + formatDouble(totalSpent) + " EGP";
				nk_label(&glfw.ctx, spentStr.c_str(), NK_TEXT_LEFT);
			}
		}
		nk_end(&glfw.ctx);

		// Add New Item Window - Bottom Right
		static char nameInput[64] = "";
		static char typeInput[64] = "";
		static char quantityInput[32] = "";
		static char priceInput[32] = "";
		static vector<string> nameSuggestions;
		static string lastNameInput = "";
		
		if (nk_begin(&glfw.ctx, "Add New Item", nk_rect(750, 400, 550, 700), NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
			// Tab navigation logic
			if (nk_input_is_key_pressed(&glfw.ctx.input, NK_KEY_TAB)) {
				focusedField = (focusedField + 1) % 4;
			}
		
			// --- NAME ROW ---
			nk_layout_row_dynamic(&glfw.ctx, 25, 2);
			nk_label(&glfw.ctx, "Name:", NK_TEXT_LEFT);
			
			// Detect click to focus
			struct nk_rect nameBounds = nk_widget_bounds(&glfw.ctx);
			if (nk_input_is_mouse_pressed(&glfw.ctx.input, NK_BUTTON_LEFT) && nk_input_is_mouse_hovering_rect(&glfw.ctx.input, nameBounds)) {
			    focusedField = 0;
			}
			if (focusedField == 0) nk_edit_focus(&glfw.ctx, NK_EDIT_FIELD);
			nk_edit_string_zero_terminated(&glfw.ctx, NK_EDIT_FIELD, nameInput, sizeof(nameInput), nk_filter_default);
			
			// Rebuild suggestions only when input changes
			if (lastNameInput != nameInput) {
			    lastNameInput = nameInput;
			    nameSuggestions.clear();
			    if (strlen(nameInput) > 0) {
			        stringstream ss;
			        ss << "SELECT DISTINCT name FROM purchases "
			           << "WHERE LOWER(name) LIKE LOWER('" << nameInput << "%') "
			           << "ORDER BY name COLLATE NOCASE "
			           << "LIMIT 5;";
			        sqlite3_stmt* stmt;
			        if (sqlite3_prepare_v2(db, ss.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
			            while (sqlite3_step(stmt) == SQLITE_ROW) {
			                const char* s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
			                nameSuggestions.push_back(s);
			            }
			        }
			        sqlite3_finalize(stmt);
			    }
			}
			
			// Draw suggestion list (max 5)
			if (focusedField == 0) {
				for (const string& suggestion : nameSuggestions) {
				    struct nk_rect b = nk_widget_bounds(&glfw.ctx);
				    struct nk_command_buffer* buf = nk_window_get_canvas(&glfw.ctx);
				    nk_fill_rect(buf, b, 0, nk_rgba(50, 50, 50, 255));
				    if (nk_button_label(&glfw.ctx, suggestion.c_str())) {
				        strcpy(nameInput, suggestion.c_str());
				        nameSuggestions.clear();
				        focusedField = 1; // Move focus to next field
				        break;
				    }
				}
			}
			
			// --- TYPE ROW ---
			nk_layout_row_dynamic(&glfw.ctx, 25, 2);
			nk_label(&glfw.ctx, "Type:", NK_TEXT_LEFT);
			
			struct nk_rect typeBounds = nk_widget_bounds(&glfw.ctx);
			if (nk_input_is_mouse_pressed(&glfw.ctx.input, NK_BUTTON_LEFT) && nk_input_is_mouse_hovering_rect(&glfw.ctx.input, typeBounds)) {
			    focusedField = 1;
			}
			if (focusedField == 1) nk_edit_focus(&glfw.ctx, NK_EDIT_FIELD);
			nk_edit_string_zero_terminated(&glfw.ctx, NK_EDIT_FIELD, typeInput, sizeof(typeInput), nk_filter_default);
			
			// --- QUANTITY ROW ---
			nk_layout_row_dynamic(&glfw.ctx, 25, 2);
			nk_label(&glfw.ctx, "Quantity:", NK_TEXT_LEFT);
			
			struct nk_rect qtyBounds = nk_widget_bounds(&glfw.ctx);
			if (nk_input_is_mouse_pressed(&glfw.ctx.input, NK_BUTTON_LEFT) && nk_input_is_mouse_hovering_rect(&glfw.ctx.input, qtyBounds)) {
			    focusedField = 2;
			}
			if (focusedField == 2) nk_edit_focus(&glfw.ctx, NK_EDIT_FIELD);
			nk_edit_string_zero_terminated(&glfw.ctx, NK_EDIT_FIELD, quantityInput, sizeof(quantityInput), nk_filter_float);
			
			// --- PRICE ROW ---
			nk_layout_row_dynamic(&glfw.ctx, 25, 2);
			nk_label(&glfw.ctx, "Price:", NK_TEXT_LEFT);
			
			struct nk_rect priceBounds = nk_widget_bounds(&glfw.ctx);
			if (nk_input_is_mouse_pressed(&glfw.ctx.input, NK_BUTTON_LEFT) && nk_input_is_mouse_hovering_rect(&glfw.ctx.input, priceBounds)) {
			    focusedField = 3;
			}
			if (focusedField == 3) nk_edit_focus(&glfw.ctx, NK_EDIT_FIELD);
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
					selectionType = SelectionType::Row;
					selectedValue = "";
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
