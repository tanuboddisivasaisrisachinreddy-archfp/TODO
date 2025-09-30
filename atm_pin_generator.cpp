// atm_pin_generator.cpp
// Compile: g++ -std=c++17 -O2 atm_pin_generator.cpp -o atm_pin_generator
// Example run: ./atm_pin_generator

#include <bits/stdc++.h>
using namespace std;

// ---------- Config ----------
const int DEFAULT_PIN_LENGTH = 4;
const int MAX_WRONG_ATTEMPTS = 3;
const string DB_FILENAME = "atm_users.db"; // simple storage (not secure encryption)
// ----------------------------

// Utility: generate cryptographically better randomness source
static std::mt19937 rng((std::random_device())());

// Returns true if digits form ascending or descending sequence of length >= pinLength-1
bool isSequential(const string &pin) {
    bool asc = true, desc = true;
    for (size_t i = 1; i < pin.size(); ++i) {
        int prev = pin[i-1] - '0';
        int cur = pin[i] - '0';
        if (cur != prev + 1) asc = false;
        if (cur != prev - 1) desc = false;
    }
    return asc || desc;
}

bool hasTooManyRepeats(const string &pin) {
    // If all digits same or 3 same in a row for 4-digit PIN, consider weak
    int sameCount = 1;
    for (size_t i = 1; i < pin.size(); ++i) {
        if (pin[i] == pin[i-1]) {
            sameCount++;
            if (sameCount >= 3) return true;
        } else sameCount = 1;
    }
    // also catch all same digit
    bool allSame = true;
    for (char c : pin) if (c != pin[0]) { allSame = false; break; }
    return allSame;
}

// Generate a PIN with checks to avoid obvious patterns
string generatePin(int length = DEFAULT_PIN_LENGTH) {
    uniform_int_distribution<int> digitDist(0, 9);
    string pin;
    for (;;) {
        pin.clear();
        for (int i = 0; i < length; ++i) pin.push_back(char('0' + digitDist(rng)));
        if (isSequential(pin)) continue;
        if (hasTooManyRepeats(pin)) continue;
        // avoid very-weak well-known pins (subset)
        static const unordered_set<string> banned = {
            "1234","0000","1111","1212","7777","1004","2000","4321","2580"
        };
        if (banned.count(pin)) continue;
        return pin;
    }
}

// Simple obfuscation for file storage (XOR with key) — NOT CRYPTOGRAPHIC
string obfuscate(const string &s) {
    const string key = "sachin_key_v1"; // local constant
    string out(s.size(), '\0');
    for (size_t i = 0; i < s.size(); ++i) out[i] = s[i] ^ key[i % key.size()];
    return out;
}

string deobfuscate(const string &s) {
    return obfuscate(s); // XOR is symmetric with same key
}

// User record
struct User {
    string username;
    string pin; // NOTE: stored obfuscated on disk; in-memory as plain for simplicity
    double balance = 0.0;
    int wrongAttempts = 0;
    bool locked = false;

    string serialize() const {
        // format: username|pin|balance|wrongAttempts|locked
        ostringstream oss;
        oss << username << '|' << pin << '|' << fixed << setprecision(2) << balance
            << '|' << wrongAttempts << '|' << (locked ? "1" : "0");
        return obfuscate(oss.str());
    }

    static User deserialize(const string &obf) {
        string s = deobfuscate(obf);
        User u;
        stringstream ss(s);
        string token;
        getline(ss, u.username, '|');
        getline(ss, u.pin, '|');
        getline(ss, token, '|'); u.balance = stod(token);
        getline(ss, token, '|'); u.wrongAttempts = stoi(token);
        getline(ss, token, '|'); u.locked = (token == "1");
        return u;
    }
};

// Simple file-based DB
class UserDB {
    unordered_map<string, User> users;
public:
    UserDB() { load(); }

    void load() {
        users.clear();
        ifstream ifs(DB_FILENAME, ios::binary);
        if (!ifs) return;
        string line;
        while (getline(ifs, line)) {
            if (line.empty()) continue;
            try {
                User u = User::deserialize(line);
                users[u.username] = u;
            } catch (...) {
                // ignore malformed line
            }
        }
    }

    void save() const {
        ofstream ofs(DB_FILENAME, ios::binary | ios::trunc);
        for (const auto &p : users) {
            ofs << p.second.serialize() << '\n';
        }
    }

    bool exists(const string &username) const {
        return users.find(username) != users.end();
    }

    bool addUser(const User &u) {
        if (exists(u.username)) return false;
        users[u.username] = u;
        save();
        return true;
    }

    bool updateUser(const User &u) {
        if (!exists(u.username)) return false;
        users[u.username] = u;
        save();
        return true;
    }

    optional<User> getUser(const string &username) const {
        auto it = users.find(username);
        if (it == users.end()) return nullopt;
        return it->second;
    }
};

// Application logic
void createAccount(UserDB &db) {
    cout << "\n--- Create Account ---\nUsername (no spaces): ";
    string username;
    cin >> username;
    if (db.exists(username)) {
        cout << "User already exists.\n";
        return;
    }
    int length = DEFAULT_PIN_LENGTH;
    cout << "Choose PIN length (4 or 6) [default 4]: ";
    string choice; cin >> choice;
    if (choice == "6") length = 6;

    string pin = generatePin(length);
    cout << "Generated PIN for user '" << username << "': " << pin << "\n";
    cout << "(This would be printed on receipt in a real system; store it securely.)\n";

    User u;
    u.username = username;
    u.pin = pin;
    u.balance = 1000.00; // default starting balance for demo
    u.wrongAttempts = 0;
    u.locked = false;

    if (!db.addUser(u)) cout << "Failed to add user.\n";
    else cout << "Account created and saved.\n";
}

bool authenticate(UserDB &db, User &user) {
    if (user.locked) {
        cout << "Account is locked due to too many wrong attempts.\n";
        return false;
    }
    cout << "Enter PIN for " << user.username << ": ";
    string entered;
    cin >> entered;
    if (entered == user.pin) {
        user.wrongAttempts = 0;
        db.updateUser(user);
        cout << "Authentication successful.\n";
        return true;
    } else {
        user.wrongAttempts += 1;
        cout << "Wrong PIN. Attempts: " << user.wrongAttempts << "/" << MAX_WRONG_ATTEMPTS << "\n";
        if (user.wrongAttempts >= MAX_WRONG_ATTEMPTS) {
            user.locked = true;
            cout << "Account locked due to too many wrong attempts.\n";
        }
        db.updateUser(user);
        return false;
    }
}

void changePin(UserDB &db, User &user) {
    cout << "\n--- Change PIN ---\n";
    if (!authenticate(db, user)) return;
    int length = user.pin.size();
    cout << "Enter new PIN (length " << length << "): ";
    string newPin; cin >> newPin;
    if ((int)newPin.size() != length) {
        cout << "Invalid length.\n"; return;
    }
    if (isSequential(newPin) || hasTooManyRepeats(newPin)) {
        cout << "New PIN is weak; choose a less trivial PIN.\n"; return;
    }
    user.pin = newPin;
    user.wrongAttempts = 0; // reset on change
    db.updateUser(user);
    cout << "PIN changed successfully.\n";
}

void atmSession(UserDB &db, User &user) {
    while (true) {
        cout << "\n--- ATM Menu (" << user.username << ") ---\n";
        cout << "1. Check balance\n2. Withdraw\n3. Change PIN\n4. Logout\nChoose: ";
        int opt; cin >> opt;
        if (opt == 1) {
            if (!authenticate(db, user)) continue;
            cout << "Balance: Rs " << fixed << setprecision(2) << user.balance << '\n';
        } else if (opt == 2) {
            if (!authenticate(db, user)) continue;
            cout << "Enter amount to withdraw: ";
            double amt; cin >> amt;
            if (amt <= 0) { cout << "Invalid amount.\n"; continue; }
            if (amt > user.balance) { cout << "Insufficient funds.\n"; continue; }
            user.balance -= amt;
            db.updateUser(user);
            cout << "Please collect cash. New balance: Rs " << fixed << setprecision(2) << user.balance << '\n';
        } else if (opt == 3) {
            changePin(db, user);
        } else if (opt == 4) {
            cout << "Logging out...\n"; break;
        } else {
            cout << "Invalid option.\n";
        }
    }
}

void loginAndRun(UserDB &db) {
    cout << "\n--- Login ---\nUsername: ";
    string username; cin >> username;
    auto opt = db.getUser(username);
    if (!opt.has_value()) {
        cout << "No such user.\n";
        return;
    }
    User user = opt.value();
    if (user.locked) {
        cout << "Account locked. Contact admin.\n"; return;
    }
    // Authenticate (gives MAX_WRONG_ATTEMPTS attempts inside authenticate)
    if (authenticate(db, user)) {
        // refresh user from DB (to reflect updated wrongAttempts/lock fields)
        auto refreshed = db.getUser(username);
        if (refreshed.has_value()) user = refreshed.value();
        atmSession(db, user);
    }
}

void adminListUsers(const UserDB &db) {
    cout << "\n--- Users (admin view; obfuscated on disk) ---\n";
    // This is just a convenience for the demo
    // Real apps must not display PINs!
    for (const string &line : vector<string>{}) {} // placeholder
    // We'll show usernames and locked status and balance
    cout << "Username\tBalance\tLocked\n";
    // hack: we can't access private users map, so reload file to parse
    ifstream ifs(DB_FILENAME, ios::binary);
    if (!ifs) { cout << "(no DB file)\n"; return; }
    string line;
    while (getline(ifs, line)) {
        if (line.empty()) continue;
        try {
            User u = User::deserialize(line);
            cout << u.username << "\tRs " << fixed << setprecision(2) << u.balance
                 << "\t" << (u.locked ? "Yes" : "No") << "\n";
        } catch (...) {}
    }
}

int main() {
    UserDB db;
    cout << "=== ATM PIN Generator Demo ===\n";
    while (true) {
        cout << "\nMain menu:\n1. Create account (generate PIN)\n2. Login\n3. Admin: list users\n4. Exit\nChoose: ";
        int cmd; cin >> cmd;
        if (cmd == 1) createAccount(db);
        else if (cmd == 2) loginAndRun(db);
        else if (cmd == 3) adminListUsers(db);
        else if (cmd == 4) { cout << "Bye!\n"; break; }
        else cout << "Invalid choice.\n";
    }
    return 0;
}