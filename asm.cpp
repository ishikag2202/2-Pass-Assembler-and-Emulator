#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
using namespace std;

//Structure to store details of a warning
struct WarningDetails {
    int position;    // The position (line number or location) where the warning occurred
    string message;  // The warning message describing the issue

    // Overloading the '<' operator to allow sorting warnings by position (ascending order)
    bool operator< (const WarningDetails &other) {
        return position < other.position;  // Compare position to sort warnings
    }
};

//Structure to store details of an error
struct ErrorDetails {
    int position;    // The position (line number or location) where the error occurred
    string message;  // The error message describing what went wrong

    // Overloading the '<' operator to allow sorting errors by position (ascending order)
    bool operator< (const ErrorDetails &other) {
        return position < other.position;  // Compare position to sort errors
    }
};

//Structure to store details for listing file generation
struct ListingDetails {
    string address;      // The address (program counter value) in the listing file
    string machineCode;  // The corresponding machine code (in hex format)
    string statement;    // The statement or source code associated with the machine code
};

//Structure to store details of a program line, associated with the program counter (PC)
struct LineDetails {
    int programCounter;   // The program counter value corresponding to this line in the program
    string label;         // The label associated with the line
    string instruction;   // The mnemonic/instruction
    string operand;       // The operand used with the instruction 
    string previousOperand; // The operand used in the previous instruction (for comparison)
};

// Containers to store different information related to errors, warnings, lines, and listings
vector<WarningDetails> warningList;          // List to store all warnings encountered
vector<ErrorDetails> errorList;              // List to store all errors encountered
vector<ListingDetails> listingEntries;       // List to store the generated listing file entries
vector<LineDetails> lineRecords;             // List to store program line information
vector<std::string> machineCodeList;         // List to store machine codes in 8-bit hexadecimal format

// Function to add a warning to the warning list
void addWarnings(int location, string message) {
    warningList.push_back({location, message});  // Add a new warning with its location and message
}

// Function to add an error to the error list
void addErrors(int location, string message) {
    errorList.push_back({location, message});  // Add a new error with its location and message
}

vector<pair<string, pair<int, int>>> symbolTable;  // {label, {address, lineNum}}
vector<pair<int, string>> commentLines;            // {line, comment}
vector<pair<string, pair<string, int>>> opcodeTable;  // {mnemonic, {opcode, operand type}}
vector<pair<string, vector<int>>> labelReferences;  // {label, {list of line numbers where the label is used}}
vector<pair<string, string>> variableAssignments;   // {variable(label), associated value}

void fillOpcodeTable() {
    // third argument if type of operand->
    //  type 0 : nothing required
    //  type 1 : value required
    //  type 2 : offset required
    opcodeTable = {
        {"data", {"", 1}}, {"ldc", {"00", 1}}, {"adc", {"01", 1}}, {"ldl", {"02", 2}},
        {"stl", {"03", 2}}, {"ldnl", {"04", 2}}, {"stnl", {"05", 2}}, {"add", {"06", 0}},
        {"sub", {"07", 0}}, {"shl", {"08", 0}}, {"shr", {"09", 0}}, {"adj", {"0A", 1}},
        {"a2sp", {"0B", 0}}, {"sp2a", {"0C", 0}}, {"call", {"0D", 2}}, {"return", {"0E", 0}},
        {"brz", {"0F", 2}}, {"brlz", {"10", 2}}, {"br", {"11", 2}}, {"HALT", {"12", 0}},
        {"SET", {"", 1}}
    };
}

vector<string> parseLine(string currentLine, int locationCounter) {  
    // If the line is empty, return an empty vector as no information can be extracted
    if (currentLine.empty()) return {};
    vector<string> result;  // This will hold the parsed words
    stringstream now(currentLine);  // Stringstream to extract words from the line
    string word;
    // Process each word in the current line
    while (now >> word) {
        if (word.empty()) continue;  // Skip empty words (spaces or tabs)
        // If a comment (denoted by ';') is encountered, stop processing further words
        if (word[0] == ';') break;
        // Check if the word contains a ':' and handle the case where ':' is not properly separated from the statement
        auto i = word.find(':');
        if (i != string::npos && word.back() != ':') {  
            result.push_back(word.substr(0, i + 1));  // Add the part before ':' to result
            word = word.substr(i + 1);  // Update the word by removing the part before ':'
        }
        // Handle case where ';' is attached directly to the word, without space
        if (word.back() == ';') {  
            word.pop_back();  // Remove the trailing ';'
            result.push_back(word);  // Add the word without ';'
            break;  // End parsing as the line likely ends with a statement followed by ';'
        }
        result.push_back(word);  // Add the word to the result if it doesn’t end with a semicolon
    }
    // Initialize a string to store the comment (if any)
    string comment = "";
    // Look for the comment in the line, denoted by ';' and extract everything after it
    for (int i = 0; i < (int)currentLine.size(); ++i) {
        if (currentLine[i] == ';') {
            int j = i + 1;  // Start from the character after ';'
            // Skip any leading spaces after the semicolon
            while (j < currentLine.size() && currentLine[j] == ' ') ++j;

            // Append remaining characters to comment string
            for (; j < currentLine.size(); ++j) {
                comment += currentLine[j];
            }
            break;  // Stop once the comment is fully extracted
        }
    }
    // If a comment is found, store it as a pair (line number, comment) in commentLines
    if (!comment.empty()) {
        commentLines.push_back({locationCounter, comment});
    }
    return result;  // Return the parsed words
}


class Validator {
public:
    // Check if the character is a digit (0-9)
    bool isDigit(char ch) {
        return ch >= '0' && ch <= '9';  // Checks if the character is between '0' and '9'
    }
    // Check if the character is an alphabet (a-z or A-Z)
    bool isAlphabet(char ch) {
        char lowerCh = tolower(ch);  // Convert character to lowercase to handle both upper and lowercase letters
        return lowerCh >= 'a' && lowerCh <= 'z';  // Checks if the character is between 'a' and 'z'
    }
    // Validate if the label is correct
    // A valid label must start with an alphabet and can contain digits, alphabets, and underscores
    bool isValidLabel(string label) {  
        bool valid = true;
        // The first character must be an alphabet
        valid &= isAlphabet(label[0]);
        // Every character in the label must be either a digit, alphabet, or an underscore
        for (char ch : label) {
            valid &= (isDigit(ch) || isAlphabet(ch) || (ch == '_'));
        }
        
        return valid;  // Return whether the label is valid
    }
    // Check if the string represents a decimal number (all characters should be digits)
    bool isDecimal(string number) {
        bool valid = true;
        for (char ch : number) {
            valid &= isDigit(ch);  // Check if each character is a digit
        }
        return valid;  // Return whether the number is a valid decimal
    }
    // Check if the string represents an octal number (starts with '0' and contains digits between 0-7)
    bool isOctal(string number) {
        bool valid = true;
        // The number should be at least two characters long and start with '0'
        valid &= (number.size() >= 2 && number[0] == '0');
        // Every character in the number should be between '0' and '7'
        for (char ch : number) {
            valid &= (ch >= '0' && ch <= '7');
        }
        return valid;  // Return whether the number is a valid octal
    }
    // Check if the string represents a hexadecimal number (starts with '0x' and contains valid hexadecimal characters)
    bool isHexadecimal(string number) {
        bool valid = true;        
        // The number should be at least 3 characters long and start with "0x"
        valid &= (number.size() >= 3 && number[0] == '0' && tolower(number[1]) == 'x');
        // Every character in the number after "0x" should be either a digit or a valid hexadecimal character (a-f or A-F)
        for (int i = 2; i < number.size(); ++i) {
            valid &= isDigit(number[i]) || (tolower(number[i]) >= 'a' && tolower(number[i]) <= 'f');
        }
        return valid;  // Return whether the number is a valid hexadecimal
    }
};
Validator validator;

class Converter {
public:
    // Convert octal to decimal
    string octalToDec(string num) {  
        int result = 0;
        // Iterate over the octal number from right to left
        for (int i = num.size() - 1, power = 1; i >= 0; --i, power *= 8) {
            // Convert each digit to decimal and add it to the result
            result += power * (num[i] - '0');
        }
        // Return the result as a string
        return to_string(result);
    }
    // Convert hexadecimal to decimal
    string hexToDec(string num) {
        int result = 0;
        // Iterate over the hexadecimal number from right to left
        for (int i = num.size() - 1, power = 1; i >= 0; --i, power *= 16) {
            // If the character is a digit, subtract '0'; if it's a letter, subtract 'a' and add 10 (for a-f)
            result += power * (validator.isDigit(num[i]) ? (num[i] - '0') : (tolower(num[i]) - 'a') + 10);
        }
        // Return the result as a string
        return to_string(result);
    }
    // Convert decimal to 8-bit hexadecimal string
    string decToHex(int num) {
        unsigned int number = num;
        string result = "";
        // Convert the decimal number to hexadecimal (8 bits)
        for (int i = 0; i < 8; ++i, number /= 16) {
            // Find the remainder when divided by 16 (hexadecimal digits)
            int remainder = number % 16;
            
            // If the remainder is 0-9, convert it to the character '0'-'9'
            // If the remainder is 10-15, convert it to the character 'A'-'F'
            result += (remainder <= 9 ? char(remainder + '0') : char(remainder - 10) + 'A');
        }
        // Reverse the result string as the conversion gives the hexadecimal digits in reverse order
        reverse(result.begin(), result.end());
        // Return the 8-bit hexadecimal string
        return result;
    }
};

Converter converter;

// Process labels and check for errors
void LabelProcessor(string label, int location_counter, int program_counter) {
    // If the label is empty, return immediately as there's nothing to process
    if (label.empty()) return;
    // Validate the label using the Validator class (check if it's a valid label)
    bool isValid = validator.isValidLabel(label);
    if (!isValid) {
        // If the label is not valid, report an error with the location counter
        addErrors(location_counter, "Bogus Label name");
    } else {
        bool labelExists = false;
        // Check if the label already exists in the symbol table
        for (auto &entry : symbolTable) {
            if (entry.first == label) {  // If the label is found in the symbol table
                if (entry.second.first != -1) {
                    // If the label already has a valid program counter, it's a duplicate definition
                    addErrors(location_counter, "Duplicate label definition");
                    labelExists = true;
                } else {
                    // If the label exists but hasn't been defined yet, update its program counter and location counter
                    entry.second = {program_counter, location_counter};
                }
                break;  // Exit the loop once the label is found and processed
            }
        }
        // If the label wasn't found in the symbol table, add a new entry with the label, program counter, and location counter
        if (!labelExists) {
            symbolTable.push_back({label, {program_counter, location_counter}});
        }
    }
}

string OperandProcessor(string operand, int location_counter) {
    string result = "";  // Initialize the return string which will store the processed operand
    // Check if the operand is a valid label using the Validator class
    if (validator.isValidLabel(operand)) {
        bool labelFound = false;
        // Search if the label already exists in labelReferences
        for (auto &entry : labelReferences) {
            if (entry.first == operand) {
                // If the label is found, append the current location counter to its reference list
                entry.second.push_back(location_counter);
                labelFound = true;
                break;  // Exit the loop once the label is found
            }
        }
        // If the label was not found in labelReferences, add a new entry for it
        if (!labelFound) {
            labelReferences.push_back({operand, {location_counter}});
        }
        bool labelExists = false;
        // Check if the operand (label) already exists in the symbol table
        for (auto &entry : symbolTable) {
            if (entry.first == operand) {
                labelExists = true;
                break;  // Exit the loop once the label is found in the symbol table
            }
        }
        // If the operand (label) is not found in symbolTable, add it with a placeholder (-1 program counter)
        if (!labelExists) {
            symbolTable.push_back({operand, {-1, location_counter}});  // Label is used but not yet defined
        }
        // Return the operand as is if it's a valid label
        return operand;
    }
    // If the operand is not a valid label, process it as a numeric value (octal, hexadecimal, or decimal)
    string now = operand, sign = "";
    // Handle the case where the operand has a sign (+ or -)
    if (now[0] == '-' or now[0] == '+') {
        sign = now[0];  // Store the sign
        now = now.substr(1);  // Remove the sign from the operand for further processing
    }
    result += sign;  // Add the sign back to the result
    // Handle different operand formats: Octal, Hexadecimal, and Decimal
    if (validator.isOctal(now)) {
        // If the operand is in octal, convert it to decimal (removing the leading '0')
        result += converter.octalToDec(now.substr(1));
    } else if (validator.isHexadecimal(now)) {
        // If the operand is in hexadecimal, convert it to decimal (removing the leading '0x')
        result += converter.hexToDec(now.substr(2));
    } else if (validator.isDecimal(now)) {
        // If the operand is already a decimal number, simply return it as a string
        result=result+now;
    } else {
        // If the operand format is invalid, return an empty string
        result = "";
    }
    return result;  // Return the processed operand (or an empty string if invalid)
}


//Finding errors related to Mnemonics
void MnemonicProcessor(string instruction_name, string &operand, int location_counter, int program_counter, int rem, bool &flag) {
    if (instruction_name.empty()) return;  // If the instruction name is empty, there is nothing to process
    bool foundMnemonic = false;  // Flag to check if the mnemonic is found in the opcode table
    int type = -1;  // Variable to store the type of the mnemonic (used to decide operand handling)
    // Searching for the instruction name (mnemonic) in the opcodeTable
    for (const auto &entry : opcodeTable) {
        if (entry.first == instruction_name) {  // If the mnemonic is found
            foundMnemonic = true;  // Mark the mnemonic as found
            type = entry.second.second;  // Get the type of the mnemonic (based on the second pair in the entry)
            break;  // Exit the loop once the mnemonic is found
        }
    }

    // If the mnemonic is not found in the opcode table, log an error for the location
    if (!foundMnemonic) {
        addErrors(location_counter, "Bogus Mnemonic");
    } else {
        // Check if the operand is present
        int isOp = !operand.empty();  // Boolean flag to check if the operand is not empty
        // If the mnemonic type requires an operand (type > 0)
        if (type > 0) {
            if (!isOp) {
                // If operand is missing, log an error
                addErrors(location_counter, "Missing operand");
            } else if (rem > 0) {
                // If there is extra content after the operand (indicated by rem), log an error
                addErrors(location_counter, "Extra on end of line");
            } else {
                // Process the operand (check if it's valid)
                string replaceOP = OperandProcessor(operand, location_counter);
                if (replaceOP.empty()) {
                    // If the operand is invalid, log an error
                    addErrors(location_counter, "Invalid format: not a valid label or a number");
                } else {
                    // If the operand is valid, update the operand and set the flag
                    operand = replaceOP;
                    flag = true;
                }
            }
        } else if (type == 0 && isOp) {
            // If the mnemonic type is 0 (indicating it shouldn't have an operand) but an operand is provided, log an error
            addErrors(location_counter, "Unexpected operand");
        } else {
            // If everything is correct, set the flag to indicate no issues
            flag = true;
        }
    }
}


//Perform the first pass of the assembler to process lines and check for label and operand errors
void first_pass(const vector<string>& readLines) {
    int location_counter = 0, program_counter = 0;
    // Process each line in the input (readLines)
    for (string curLine : readLines) {
        ++location_counter;  // Increment location counter (tracks line number)
        // Parse the current line into components (label, mnemonic, operand)
        auto cur = parseLine(curLine, location_counter);  
        if (cur.empty()) continue;  // Skip empty lines after parsing
        string label = "", instruction_name = "", operand = "";
        int pos = 0, sz = cur.size();
        // Process the label (if present) and remove the trailing colon (':')
        if (cur[pos].back() == ':') {
            label = cur[pos];           // Store the label
            label.pop_back();           // Remove the colon (':') at the end of the label
            ++pos;                      // Move to next token
        }
        // Process the mnemonic (instruction name) if it exists
        if (pos < sz) {
            instruction_name = cur[pos];  // Store the mnemonic
            ++pos;                 // Move to next token
        }
        // Process the operand (if it exists)
        if (pos < sz) {
            operand = cur[pos];   // Store the operand
            ++pos;                // Move to next token
        }
        // Process the label (check for errors related to labels)
        LabelProcessor(label, location_counter, program_counter);
        bool flag = false;  // Flag to track if the operand is valid or not
        string prevOperand = operand;  // Store the original operand for later use (in case it's modified)
        // Process the mnemonic and operand, checking for errors like missing operands or extra content
        MnemonicProcessor(instruction_name, operand, location_counter, program_counter, sz - pos, flag);
        // Record the current line details (for use in second pass, such as generating machine code)
        lineRecords.push_back({program_counter, label, instruction_name, operand, prevOperand});
        // If the mnemonic is valid, increment the program counter (advance to next instruction)
        program_counter += flag;
        // Handle "SET" instructions (used for variable assignments or label definitions)
        if (flag && instruction_name == "SET") {
            // If the label is missing in the SET instruction, add an error
            if (label.empty()) {
                addErrors(location_counter,"label(or variable) name missing");
            } else {
                // Store SET instruction information (label and operand) for later processing
                variableAssignments.push_back({label, operand});
            }
        }
    }
    // After processing all lines, check for errors related to undefined labels
    for (auto label : symbolTable) {
        bool labelUsed = false;
        
        // Check if the label is used in any instruction (check label references)
        for (const auto& labelRef : labelReferences) {
            if (labelRef.first == label.first) {
                labelUsed = true;
                break;  // Label is used, no need to check further
            }
        }
        // If the label's address is still -1, it is undefined
        if (label.second.first == -1) {
            // Report errors for all lines that refer to this undefined label
            for (const auto& labelRef : labelReferences) {
                if (labelRef.first == label.first) {
                    for (int line : labelRef.second) {
                        addErrors(line,"no such label");  // Report error for each usage of the undefined label
                    }
                    break;
                }
            }
        } else if (!labelUsed) {
            // If the label is declared but never used, add a warning
            warningList.push_back({label.second.second, "Label declared but not used"});
        }
    }
}

// Inserting the current program counter, machine code, and source line details into the listingEntries vector
void add_in_list(int program_counter, string machine_code, string label, string mnemonic, string operand) {
    // If mnemonic is not empty, add a space to it (standard mnemonic format)
    if (!mnemonic.empty()) mnemonic += " ";
    // If label is not empty, add a colon to it (standard label format)
    if (!label.empty()) label += ": ";
    // Combine label, mnemonic, and operand to form the complete source line statement
    string statement = label + mnemonic + operand;
    // Convert the program counter (which is in decimal) to hexadecimal format for machine code listing
    // Then, add the current program counter in hex, the machine code, and the statement to the listingEntries vector
    listingEntries.push_back({converter.decToHex(program_counter), machine_code, statement});
}

// Generating machine codes and building the listing vector
void second_pass() {
    // Iterate through each line record
    for (auto curLine : lineRecords) {
        // Extract label, mnemonic, operand, and previous operand for the current line
        string label = curLine.label, mnemonic = curLine.instruction, operand = curLine.operand;
        string prevOperand = curLine.previousOperand;
        int program_counter = curLine.programCounter, type = -1;
        string opcode = "";
        // Find mnemonic in opcodeTable to retrieve its type and corresponding opcode
        for (const auto& opcodeEntry : opcodeTable) {
            if (opcodeEntry.first == mnemonic) {
                opcode = opcodeEntry.second.first;  // Retrieve opcode
                type = opcodeEntry.second.second;   // Retrieve type of mnemonic (e.g., value/offset)
                break;
            }
        }
        string machineCode = "        ";  // Default empty machine code to start with
        // If the mnemonic type requires an offset (e.g., branch instructions)
        if (type == 2) {  
            int offset = -1;
            bool found = false;
            // Look for the label in symbolTable to calculate the offset
            for (const auto& sym : symbolTable) {
                if (sym.first == operand) {
                    offset = sym.second.first - (program_counter + 1);  // Calculate offset based on symbol's address
                    found = true;
                    break;
                }
            }
            // If label not found, treat the operand as an immediate value
            if (!found) {
                offset = stoi(operand);  // Convert operand to an integer if it's not a label
            }
            // Convert the offset to hexadecimal and concatenate with the opcode
            machineCode = converter.decToHex(offset).substr(2) + opcode;
        }
        // If mnemonic requires a value (e.g., arithmetic or memory instructions)
        else if (type == 1 && mnemonic != "data" && mnemonic != "SET") {  
            int value = -1;
            bool found = false;
            // Look for the label in symbolTable to retrieve its value
            for (const auto& sym : symbolTable) {
                if (sym.first == operand) {
                    value = sym.second.first;  // Retrieve the value of the label from symbolTable
                    found = true;
                    break;
                }
            }
            // If label is not found, treat the operand as an immediate value
            if (!found) {
                value = stoi(operand);  // Convert operand to integer
            }
            // Convert the value to hexadecimal and concatenate with the opcode
            machineCode = converter.decToHex(value).substr(2) + opcode;

            // Check if operand is a variable in SET operation and use its assigned value
            auto it = find_if(variableAssignments.begin(), variableAssignments.end(),
                [&operand](const std::pair<std::string, std::string>& item) { 
                    return item.first == operand; 
                });

            // If the operand is a variable in the SET operation, use its assigned value
            if (it != variableAssignments.end()) {
                machineCode = converter.decToHex(stoi(it->second)).substr(2) + opcode;
            }
        }
        // For type 0 mnemonics (no operands, like "HALT"), just append the opcode
        else if (type == 0) {  
            machineCode = "000000" + opcode;  // No operand, set to default zero with opcode
        }
        // Special case for "data" and "SET" instructions, where operand is directly converted
        else if (type == 1 && (mnemonic == "data" || mnemonic == "SET")) {  
            machineCode = converter.decToHex(stoi(operand));  // Convert the operand directly to hexadecimal
        }
        // Add the generated machine code to the list for later processing
        machineCodeList.emplace_back(machineCode);
        // Add the current program counter, machine code, label, mnemonic, and previous operand to the listing
        add_in_list(program_counter, machineCode, label, mnemonic, prevOperand);
    }
}



// Function to write errors and warnings into a .log file
void show_warnings_and_errors() {
    // Open a log file to write errors and warnings
    ofstream coutErrors("logfile.log");
    // Sort both error and warning lists based on line position for ordered output
    sort(errorList.begin(), errorList.end());
    sort(warningList.begin(), warningList.end());
    // Notify user that the error log file has been generated
    cout << "Errors (.log) file has been created." << endl;
    // Check if there are any errors in errorList
    if (errorList.empty()) {
        // If no errors, write a "No errors!" message to the log file
        coutErrors << "No errors found!!" << endl;
        // Write all warnings to the log file, if any
        for (auto warning : warningList) {
            coutErrors << "Line Number:- " << warning.position << " WARNING:- " << warning.message << endl;
        }
        // Close the file and return since there are no errors to write
        coutErrors.close();
        return;
    }
    // If errors are present, write each error to the log file
    for (auto error : errorList) {
        coutErrors << "Line Number:- " << error.position << " ERROR:- " << error.message << endl;
    }
    // Close the log file after writing all errors (and warnings if any)
    coutErrors.close();
}


vector<string> readLines; 		// stores each line 

// Reading from the input file
// Function to read lines from "bubbleSort.txt" file and store them in readLines vector
void readFile() {
    ifstream cinfile;  // Create an input file stream
    cinfile.open("fib.txt");  // Open the file "bubbleSort.txt" for reading

    // Check if file opening failed
    if (cinfile.fail()) {
        cout << "Input file doesn't exist" << endl;  // Print error message if file is not found
        exit(0);  // Exit the program
    }

    string curLine;
    // Read each line from the file until end of file is reached
    while (getline(cinfile, curLine)) {
        readLines.push_back(curLine);  // Add each line to the readLines vector
    }

    cinfile.close();  // Close the file after reading all lines
}


// Function to write listing information to a .lst file and machine code to a .o binary file
void writeFile() {
    // Write listing information to .lst file
    ofstream coutList("listfile.lst");  // Create an output file stream for the .lst file
    for (auto entry : listingEntries) {
        // Write each entry with address, machine code, and statement to the .lst file
        coutList << entry.address << " " << entry.machineCode << " " << entry.statement << endl;
    }
    coutList.close();  // Close the .lst file after writing all entries
    cout << "Listing (.lst) file generated" << endl;
    // Write machine code to .o binary file
    ofstream coutMCode;
    coutMCode.open("machineCode.o", ios::binary | ios::out);  // Open the .o file in binary write mode
    for (auto code : machineCodeList) {
        // Skip empty or placeholder machine codes
        if (code.empty() || code == "        ") continue;

        // Convert hex string to an unsigned integer for binary writing
        unsigned int machineCode = static_cast<unsigned int>(stoi(converter.hexToDec(code)));

        // Write the binary representation of machineCode to the .o file
        coutMCode.write(reinterpret_cast<const char*>(&machineCode), sizeof(unsigned int));
    }
    coutMCode.close();  // Close the .o file after writing all machine codes
    cout << "Machine code object (.o) file generated" << endl;
}

int main() {
   readFile();
   fillOpcodeTable();
   first_pass(readLines);
   show_warnings_and_errors();
   if(errorList.empty()){
    second_pass();
    writeFile();
   }
   return 0;
}
