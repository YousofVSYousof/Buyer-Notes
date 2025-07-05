#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <ctime>
#include <vector>
#include <iomanip>

using namespace std;

struct Purchase {
	int id;
	string name;
	double quantity;
	double price;
	string timeStamp;
	string type;
};

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

string getTimeStamp() {
	time_t now = time(nullptr);
	tm *ltm = localtime(&now);
	char buffer[20];
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);
	return string(buffer);
}

void insert(sqlite3 *db, string name, double quantity, double price, string type = "") {
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

		if (type.empty()) {
			type = "food";
		}
	}

	stringstream command;
	command << "INSERT INTO purchases (name, quantity, price, timeStamp, type) VALUES ('"
			<< name << "', "
			<< quantity << ", "
			<< price << ", '"
			<< getTimeStamp() << "', '"
			<< type << "');";
	runCommand(db, command.str());
}

double getTotalSpent(sqlite3 *db, string item, int days) {
	stringstream command;
	command << "SELECT SUM(quantity * price) FROM purchases "
			<< "WHERE name = '" << item << "' "
			<< "AND timeStamp >= date('now', '-" << days << " day');";

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

double getTotalSpent(sqlite3* db, int days) {
	stringstream command;
	command << "SELECT SUM(quantity * price) FROM purchases "
			<< "WHERE timeStamp >= date('now', '-" << days << " day');";

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

double getTotalSpent(sqlite3* db) {
	string command = "SELECT SUM(quantity * price) FROM purchases;";
	sqlite3_stmt* stmt;
	double total = 0;

	if (sqlite3_prepare_v2(db, command.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
			total = sqlite3_column_double(stmt, 0);
		}
	}
	sqlite3_finalize(stmt);
	
	return total;
}

double getAverageSpentPerDayLastMonth(sqlite3* db) {
	stringstream query;
	query << "SELECT AVG(daily_total) FROM ("
		  << "SELECT date(timeStamp) as day, SUM(quantity * price) as daily_total "
		  << "FROM purchases WHERE timeStamp >= date('now', '-30 day') "
		  << "GROUP BY day"
		  << ");";

	sqlite3_stmt* stmt;
	double avg = 0.0;
	if (sqlite3_prepare_v2(db, query.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
			avg = sqlite3_column_double(stmt, 0);
		}
	}
	sqlite3_finalize(stmt);
	
	return avg;
}

double getTotalQuantity(sqlite3 *db, string item, int days) {
	stringstream command;
	command << "SELECT SUM(quantity) FROM purchases "
			<< "WHERE name = '" << item << "' "
			<< "AND timeStamp >= date('now', '-" << days << " day');";

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

double getTotalSpentOnDate(sqlite3* db, string timeStamp) {
	string datePart = timeStamp.substr(0, 10);
	stringstream command;
	command << "SELECT SUM(quantity * price) FROM purchases "
			<< "WHERE substr(timeStamp, 1, 10) = '" << datePart << "';";

	sqlite3_stmt* stmt;
	double total = 0.0;
	if (sqlite3_prepare_v2(db, command.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
			total = sqlite3_column_double(stmt, 0);
		}
	}
	sqlite3_finalize(stmt);
	
	return total;
}

vector<Purchase> loadPurchases(sqlite3* db) {
	vector<Purchase> purchases;
	string command = "SELECT id, name, quantity, price, timeStamp, type FROM purchases ORDER BY timeStamp DESC, id DESC;";
	sqlite3_stmt* stmt;
	
	if (sqlite3_prepare_v2(db, command.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			Purchase p;
			p.id = sqlite3_column_int(stmt, 0);
			p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
			p.quantity = sqlite3_column_double(stmt, 2);
			p.price = sqlite3_column_double(stmt, 3);
			p.timeStamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
			p.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
			purchases.push_back(p);
		}
	}
	sqlite3_finalize(stmt);
	
	return purchases;
}

string formatDouble(double value, int precision = 2) {
	ostringstream ss;
	ss << fixed << setprecision(precision) << value;
	return ss.str();
}

void deletePurchase(sqlite3 *db, int id) {
	stringstream ss;
	ss << "DELETE FROM purchases WHERE id = " << id << ";";
	runCommand(db, ss.str());
}

#endif // HELPERS_HPP
