#include <iostream>
#include <string>
#include <string_view>
#include <cctype>

using namespace std;

struct Lexer {
    string src;
    int pos;
    int line;

   // Lexer(string_view s) : src(s), pos(0), line(1) {}
   // Lexer(string s) : src(s), pos(0), line(1) {}
   
    char peek()  { return pos < (int)src.size() ? src[pos] : '\0'; }
    char peek2() { return pos+1 < (int)src.size() ? src[pos+1] : '\0'; }
    char advance() { return src[pos++]; }
    bool atEnd()   { return pos >= (int)src.size(); }

};

struct parser {
	string src;
	int pos;
	int line;
};


void print_info(std::string_view sv) {
    std::cout << "Length: " << sv.size() << " | Data: " << sv << '\n';
}

int main() {
    string str = "Hello from std::string";
    
    
    string aq = R"(
    qweqwe
    qweqwe
    qweqweqwe
    
    int a = 15;
    int b = 125;
    float c = 14.3;
    a = 35;
    )";
    
   if (!(10>15)) {
   	cout << "qweqw \n";
   } 
   
   
   
   int a = 0;
   while (a < aq.size()) {
   
  
  cout << a << " \n";
  
  	a++;
   }
    
    
    cout  << aq.size() << "\n";
    
    print_info(aq);  
    
    
    print_info(str);             
    print_info("Literal string");
    print_info({str.data(), 2}); 
}
