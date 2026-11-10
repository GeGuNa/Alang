#include <iostream>
#include <memory>  
#include <string>
#include <string_view>
#include <cctype>
#include <vector>
#include "tk.h"





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


enum class Tok {
    Int, Double, Ident, Number, String,
    Assign, Semicolon, End
};


struct Token {
    Tok kind;
    string text;
    string value;  
    bool isFloat = false; 
    int pos = 0;
};


void print_info(std::string_view sv) {
    std::cout << "Length: " << sv.size() << " | Data: " << sv << '\n';
}

int main() {

//auto qweq1qq = new parwser(); 

auto qweq1qq = std::make_unique<parwser>();
qweq1qq->qq();
 
// delete qweq1qq;
 
 
    string str = "Hello from std::string";
    
    std::vector<Token> out;

    string aq = R"(
    
    void qweqe(){ } 
    int qweqwe(){ return 15; }
     int qweqwe(int a, int b, string d){ return 15; }
    qweqwe
    qweqwe
    qweqweqwe
    
    int a = 15;
    int b = 125;
    float c = 14.3;
    str a = "qweqweqwe";
    string bbb = "zzzzzzz";
    a = 35;
    )";
    
   if (!(10>15)) {
  // 	cout << "qweqw \n";
   } 
   
   
   
   int a = 0;
   while (a < aq.size()) {
   
   char tok = aq[a];
   
   
   
   if (std::isspace(static_cast<unsigned char>(tok))) { 
   
    a++; 
   // cout << "  space \n"; 
   continue;  
   
   }
   
   
   if (std::isalpha(static_cast<unsigned char>(tok))) { 
   
   
   
   int start_id = a;
   
   
   while (a < aq.size() && std::isalnum(aq[a]))a++;
   
   string tknr = aq.substr(start_id, a - start_id);
 
 
 
                if (tknr == "int"){ 
                	out.push_back({Tok::Int, tknr,"", false, start_id}); 
                	//cout << " zzz \n"; 
                }
                else if (tknr == "double") { 
                
                 out.push_back({Tok::Double, tknr,"", false, start_id});  }
                 
                else if (tknr == "string") { 
                
                 out.push_back({Tok::String, tknr,"", false, start_id});  }     
        
        
                else if (tknr == "str") { 
                
                 out.push_back({Tok::String, tknr,"", false, start_id});  }       
        
        
        
                 
                else  { 
                out.push_back({Tok::Ident, tknr,"", false, start_id});
   				}
    //cout << "  str \n"; 
   
    // a++;

   
   
   continue;
   
   
    }
   
      
   if (tok == '"') { 
   
   
   			out.push_back({Tok::Assign, "\"","", false, a});
	a++; 
	continue; 
   
   }
   
   
   
   
   if (std::isdigit(static_cast<unsigned char>(tok))) { 
   
   int start_id = a;
   bool FLtq = false;
   
   while (a < aq.size() && std::isalnum(aq[a]))a++;
   
   
   
   
   if (aq[a] == '.') {
   		a++;
   		
//cout << "qweqwe yessssssss  \n ";

		while (a < aq.size() && std::isalnum(aq[a]))a++;
		FLtq = true;
   }
   
   string tknr = aq.substr(start_id, a - start_id);



if (FLtq)
	out.push_back({Tok::Int, tknr,"", false, a});
else 
	out.push_back({Tok::Double, tknr,"", false, a});



cout << " yessssssss enddddddddddddddddddddddddd \n ";  
	// cout << "  int \n"; 
	continue; 
   
   }
		
	if (tok == '=') { out.push_back({Tok::Assign, "=","", false, a}); ++a; continue; }
	if (tok == ';') { out.push_back({Tok::Semicolon, ";","", false, a}); ++a; continue; }
	
	
	if (tok == '+') { out.push_back({Tok::Semicolon, "+","", false, a}); ++a; continue; }
	if (tok == '-') { out.push_back({Tok::Semicolon, "-","", false, a}); ++a; continue; }
	if (tok == '*') { out.push_back({Tok::Semicolon, "*","", false, a}); ++a; continue; }
	if (tok == '/') { out.push_back({Tok::Semicolon, "/","", false, a}); ++a; continue; }
	
	if (tok == '>') { out.push_back({Tok::Semicolon, ">","", false, a}); ++a; continue; }
	if (tok == '<') { out.push_back({Tok::Semicolon, "<","", false, a}); ++a; continue; }
	
	if (tok == '(') { out.push_back({Tok::Semicolon, "(","", false, a}); ++a; continue; }
	if (tok == ')') { out.push_back({Tok::Semicolon, ")","", false, a}); ++a; continue; }
	

	
  	//cout << aq[a] << " \n";
  
  //	if (aq.size() == a) { cout << " out \n"; break; }
  	a++;
   }
    
   	//cout << aq[a] << " \n";
   	
 for (Token& t : out) {
  	cout << " type: " <<  t.text <<  " \n";
	}
   	
   	
   	//  Tok kind;
   // string text;
   // bool isFloat = false; 
   // int pos = 0;
    cout  << aq.size() << "\n";
   // 
   // print_info(aq);  
    
    
   // print_info(str);             
   // print_info("Literal string");
   // print_info({str.data(), 2}); 
}
