#include <bits/stdc++.h>
using namespace std;

namespace util {
    inline string trim(const string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        if (a == string::npos) return "";
        return s.substr(a, b - a + 1);
    }

    inline vector<string> splitCSV(const string& line) {
        vector<string> out;
        string cur; bool inQuote = false;
        for (char c : line) {
            if (c == '"') { inQuote = !inQuote; }
            else if (c == ',' && !inQuote) {
                out.push_back(trim(cur)); cur.clear();
            } else cur.push_back(c);
        }
        out.push_back(trim(cur));
        return out;
    }

    inline string toMoney(double v) {
        stringstream ss;
        ss.setf(std::ios::fixed); ss << setprecision(2) << v;
        return ss.str();
    }
}

class Security {
public:
    virtual ~Security() = default;
    virtual string symbol() const = 0;
    virtual string name() const = 0;
    virtual double price() const = 0;
    virtual void updatePrice(std::mt19937& rng) = 0; // polymorphic
};

class Stock : public Security {
    string m_symbol;
    string m_name;
    double m_price;
    double m_baseVol; 
public:
    Stock(string sym, string nm, double p, double vol)
        : m_symbol(std::move(sym)), m_name(std::move(nm)), m_price(p), m_baseVol(vol) {}

    string symbol() const override { return m_symbol; }
    string name()   const override { return m_name; }
    double price()  const override { return m_price; }

    void updatePrice(std::mt19937& rng) override {
        std::normal_distribution<double> noise(0.0, m_baseVol);
        double drift = 0.0005;  // tiny positive drift
        double change = drift + noise(rng);
        double np = m_price * (1.0 + change);
        // Clamp to a reasonable range to avoid going to zero or exploding
        np = max(1.0, min(np, m_price * 1.25));
        m_price = np;
    }
};

class Market {
    unordered_map<string, unique_ptr<Security>> m_securities;
public:
    Market() = default;

    void addSecurity(unique_ptr<Security> sec) {
        string sym = sec->symbol();
        m_securities.emplace(sym, std::move(sec));
    }

    Security* get(const string& symbol) {
        auto it = m_securities.find(symbol);
        if (it == m_securities.end()) return nullptr;
        return it->second.get();
    }

    const Security* get(const string& symbol) const {
        auto it = m_securities.find(symbol);
        if (it == m_securities.end()) return nullptr;
        return it->second.get();
    }

    const unordered_map<string, unique_ptr<Security>>& all() const {
        return m_securities;
    }

    void tick(std::mt19937& rng, int times = 1) {
        for (int t = 0; t < times; ++t) {
            for (auto& kv : m_securities) kv.second->updatePrice(rng);
        }
    }

    void list() const {
        cout << "\n--- Market ---\n";
        cout << left << setw(8) << "Symbol" << setw(24) << "Name" << right << setw(12) << "Price\n";
        cout << string(46, '-') << "\n";
        vector<const Security*> v;
        v.reserve(m_securities.size());
        for (auto& kv : m_securities) v.push_back(kv.second.get());
        sort(v.begin(), v.end(), [](auto* a, auto* b){ return a->symbol() < b->symbol(); });
        for (auto* s : v) {
            cout << left << setw(8) << s->symbol()
                 << setw(24) << s->name()
                 << right << setw(12) << util::toMoney(s->price()) << "\n";
        }
    }
};


struct Holding {
    string symbol;
    long long quantity = 0;
    double avgCost = 0.0;
};

class Portfolio {
    unordered_map<string, Holding> m_holdings;

public:
    bool has(const string& sym) const {
        return m_holdings.find(sym) != m_holdings.end();
    }

    const unordered_map<string, Holding>& all() const { return m_holdings; }

    void buy(const string& sym, long long qty, double price) {
        auto& h = m_holdings[sym];
        if (h.quantity == 0) {
            h.symbol = sym;
            h.quantity = qty;
            h.avgCost = price;
        } else {
            double totalCost = h.avgCost * h.quantity + price * qty;
            h.quantity += qty;
            h.avgCost = totalCost / static_cast<double>(h.quantity);
        }
    }

    double sell(const string& sym, long long qty, double price) {
        auto it = m_holdings.find(sym);
        if (it == m_holdings.end() || it->second.quantity < qty) {
            throw runtime_error("Not enough shares to sell.");
        }
        Holding& h = it->second;
        double profit = (price - h.avgCost) * static_cast<double>(qty);
        h.quantity -= qty;
        if (h.quantity == 0) {
            m_holdings.erase(it);
        }
        return profit;
    }

    double marketValue(const Market& mkt) const {
        double sum = 0.0;
        for (auto& kv : m_holdings) {
            auto* sec = mkt.get(kv.first);
            if (sec) sum += sec->price() * static_cast<double>(kv.second.quantity);
        }
        return sum;
    }

    double unrealizedPnL(const Market& mkt) const {
        double pnl = 0.0;
        for (auto& kv : m_holdings) {
            auto* sec = mkt.get(kv.first);
            if (!sec) continue;
            pnl += (sec->price() - kv.second.avgCost) * static_cast<double>(kv.second.quantity);
        }
        return pnl;
    }

    void clear() { m_holdings.clear(); }
};

class User {
    string m_name;
    double m_balance = 0.0;      
    double m_realizedPnL = 0.0;   
    Portfolio m_portfolio;

public:
    explicit User(string name) : m_name(std::move(name)) {}

    const string& name() const { return m_name; }
    double balance() const { return m_balance; }
    double realizedPnL() const { return m_realizedPnL; }
    const Portfolio& portfolio() const { return m_portfolio; }
    Portfolio& portfolio() { return m_portfolio; }

    void addFunds(double amount) {
        if (amount <= 0) throw runtime_error("Amount must be positive.");
        m_balance += amount;
    }

    void buy(Market& mkt, const string& sym, long long qty) {
        if (qty <= 0) throw runtime_error("Quantity must be positive.");
        Security* sec = mkt.get(sym);
        if (!sec) throw runtime_error("Symbol not found.");
        double cost = sec->price() * static_cast<double>(qty);
        if (cost > m_balance + 1e-9) throw runtime_error("Insufficient balance.");
        m_balance -= cost;
        m_portfolio.buy(sym, qty, sec->price());
    }

    void sell(Market& mkt, const string& sym, long long qty) {
        if (qty <= 0) throw runtime_error("Quantity must be positive.");
        Security* sec = mkt.get(sym);
        if (!sec) throw runtime_error("Symbol not found.");
        double proceeds = sec->price() * static_cast<double>(qty);
        double profit = m_portfolio.sell(sym, qty, sec->price());
        m_balance += proceeds;
        m_realizedPnL += profit;
    }

    double totalEquity(const Market& mkt) const {
        return m_balance + m_portfolio.marketValue(mkt);
    }

    // Persistence
    void save(const string& filename) const {
        ofstream out(filename);
        if (!out) throw runtime_error("Failed to open save file.");
        out.setf(std::ios::fixed); out << setprecision(8);

        out << m_balance << " " << m_realizedPnL << "\n";
        const auto& h = m_portfolio.all();
        out << h.size() << "\n";
        for (auto& kv : h) {
            out << kv.second.symbol << ","
                << kv.second.quantity << ","
                << kv.second.avgCost << "\n";
        }
    }

    void load(const string& filename) {
        ifstream in(filename);
        if (!in) return; 
        double bal = 0.0, rp = 0.0;
        size_t n = 0;
        if (!(in >> bal >> rp)) return;
        m_balance = bal; m_realizedPnL = rp;
        string dummy; getline(in, dummy); 
        string nline; if (!getline(in, nline)) return;
        n = static_cast<size_t>(stoull(util::trim(nline)));
        m_portfolio.clear();
        for (size_t i = 0; i < n; ++i) {
            string line; if (!getline(in, line)) break;
            auto parts = util::splitCSV(line);
            if (parts.size() != 3) continue;
            Holding h;
            h.symbol = parts[0];
            h.quantity = stoll(parts[1]);
            h.avgCost = stod(parts[2]);
            m_portfolio.buy(h.symbol, h.quantity, h.avgCost); 
        }
    }
};

class App {
    Market market;
    User user;
    mt19937 rng;
    const string saveFile = "portfolio.sav";

    static long long readLong(const string& prompt) {
        while (true) {
            cout << prompt;
            long long x;
            if (cin >> x) return x;
            cout << "Invalid number. Try again.\n";
            cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }

    static double readDouble(const string& prompt) {
        while (true) {
            cout << prompt;
            double x;
            if (cin >> x) return x;
            cout << "Invalid number. Try again.\n";
            cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }

    static string readSymbolUpper(const string& prompt) {
        cout << prompt;
        string s; cin >> s;
        for (auto& c : s) c = toupper(static_cast<unsigned char>(c));
        return s;
    }

    void seedMarket() {
        market.addSecurity(make_unique<Stock>("AAPL", "Apple Inc.",         185.00, 0.010));
        market.addSecurity(make_unique<Stock>("GOOG", "Alphabet Inc.",     2850.00, 0.012));
        market.addSecurity(make_unique<Stock>("TSLA", "Tesla Inc.",         240.00, 0.020));
        market.addSecurity(make_unique<Stock>("INFY", "Infosys Ltd.",        20.50, 0.015));
        market.addSecurity(make_unique<Stock>("RELI", "Reliance Ind.",       28.00, 0.013));
        market.addSecurity(make_unique<Stock>("NVDA", "NVIDIA Corp.",       950.00, 0.018));
        market.addSecurity(make_unique<Stock>("TCS",  "Tata Consultancy",    40.00, 0.010));
        market.addSecurity(make_unique<Stock>("HDFB", "HDFC Bank",           18.50, 0.011));
    }

    void showHeader() const {
        cout << "\n=============================================\n";
        cout << "    Virtual Stock Portfolio Simulator\n";
        cout << "=============================================\n";
        cout << "User: " << user.name() << "\n";
    }

    void showDashboard() const {
        cout << "\n--- Dashboard ---\n";
        cout << "Cash Balance   : $" << util::toMoney(user.balance()) << "\n";
        double mv = user.portfolio().marketValue(market);
        double upnl = user.portfolio().unrealizedPnL(market);
        cout << "Mkt Value      : $" << util::toMoney(mv) << "\n";
        cout << "Unrealized P/L : $" << util::toMoney(upnl) << "\n";
        cout << "Realized P/L   : $" << util::toMoney(user.realizedPnL()) << "\n";
        cout << "Total Equity   : $" << util::toMoney(user.totalEquity(market)) << "\n";
    }

    void showPortfolio() const {
        cout << "\n--- Portfolio ---\n";
        cout << left << setw(8) << "Symbol"
             << right << setw(10) << "Qty"
             << right << setw(14) << "Avg Cost"
             << right << setw(12) << "Price"
             << right << setw(14) << "Unrlzd P/L"
             << "\n";
        cout << string(58, '-') << "\n";
        vector<Holding> v;
        for (auto& kv : user.portfolio().all()) v.push_back(kv.second);
        sort(v.begin(), v.end(), [](const Holding& a, const Holding& b){ return a.symbol < b.symbol; });
        double totalUnreal = 0.0;
        for (auto& h : v) {
            auto* sec = market.get(h.symbol);
            if (!sec) continue;
            double price = sec->price();
            double pnl = (price - h.avgCost) * static_cast<double>(h.quantity);
            totalUnreal += pnl;
            cout << left << setw(8) << h.symbol
                 << right << setw(10) << h.quantity
                 << right << setw(14) << util::toMoney(h.avgCost)
                 << right << setw(12) << util::toMoney(price)
                 << right << setw(14) << util::toMoney(pnl)
                 << "\n";
        }
        cout << string(58, '-') << "\n";
        cout << right << setw(44) << "Total Unrealized: " << setw(14) << util::toMoney(totalUnreal) << "\n";
    }

    void doAddFunds() {
        double amt = readDouble("Enter amount to add: $");
        try {
            user.addFunds(amt);
            cout << "Added $" << util::toMoney(amt) << " successfully.\n";
        } catch (const exception& e) {
            cout << "Error: " << e.what() << "\n";
        }
    }

    void doBuy() {
        string sym = readSymbolUpper("Enter symbol to BUY: ");
        long long qty = readLong("Enter quantity: ");
        try {
            user.buy(market, sym, qty);
            cout << "Bought " << qty << " of " << sym << " successfully.\n";
        } catch (const exception& e) {
            cout << "Error: " << e.what() << "\n";
        }
    }

    void doSell() {
        string sym = readSymbolUpper("Enter symbol to SELL: ");
        long long qty = readLong("Enter quantity: ");
        try {
            user.sell(market, sym, qty);
            cout << "Sold " << qty << " of " << sym << " successfully.\n";
        } catch (const exception& e) {
            cout << "Error: " << e.what() << "\n";
        }
    }

    void save() {
        try {
            user.save(saveFile);
            cout << "Progress saved to " << saveFile << ".\n";
        } catch (const exception& e) {
            cout << "Save error: " << e.what() << "\n";
        }
    }

public:
    explicit App(string username)
        : user(std::move(username)),
          rng(static_cast<uint32_t>(chrono::high_resolution_clock::now().time_since_epoch().count())) {
        seedMarket();
        user.load(saveFile);
        if (user.balance() <= 1e-9 && user.portfolio().all().empty()) {
            cout << "Starting with demo funds: $10,000.00\n";
            user.addFunds(10000.0);
        }
    }

    void run() {
        bool running = true;
        while (running) {
            market.tick(rng); 

            showHeader();
            showDashboard();

            cout << "\nMenu:\n";
            cout << " 1) View Market\n";
            cout << " 2) View Portfolio\n";
            cout << " 3) Add Funds\n";
            cout << " 4) Buy Stock\n";
            cout << " 5) Sell Stock\n";
            cout << " 6) Save Progress\n";
            cout << " 7) Exit\n";
            cout << "Choose: ";

            int choice;
            if (!(cin >> choice)) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Invalid input.\n";
                continue;
            }

            switch (choice) {
                case 1: market.list(); break;
                case 2: showPortfolio(); break;
                case 3: doAddFunds(); break;
                case 4: doBuy(); break;
                case 5: doSell(); break;
                case 6: save(); break;
                case 7:
                    save();
                    cout << "Goodbye!\n";
                    running = false;
                    break;
                default:
                    cout << "Invalid choice. Try again.\n";
            }
        }
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout.tie(nullptr);

    cout.setf(std::ios::unitbuf);

    cout << "Enter your name: " << flush;
    string name;
    getline(cin, name);
    if (name.empty()) name = "Player";

    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    try {
        App app(std::move(name));
        cout << "\nLoading your simulator..." << endl;
        app.run();
    } catch (const std::exception& e) {
        cerr << "An unrecoverable error occurred: " << e.what() << endl;
        return 1;
    }

    cout << "\nProgram finished successfully.\n";
    return 0;
}
