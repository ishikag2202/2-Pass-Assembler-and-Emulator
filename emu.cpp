#include<bits/stdc++.h>
using namespace std;

vector<int> objectFile;
vector<int> memory(1<<24);
int PC=0;
int SP=0;
int regA=0;
int regB=0;
int total=0;
int stackLimit=1<<23;

vector<string> mnemonics{"ldc",
                        "adc",
                        "ldl",
                        "stl",
                        "ldnl",
                        "stnl",
                        "add",
                        "sub",
                        "shl",
                        "shr",
                        "adj",
                        "a2sp",
                        "sp2a",
                        "call",
                        "ret",
                        "brz",
                        "brlz",
                        "br",
                        "HALT"};

enum Opcode {
    ldc= 0,
    adc= 1,
    ldl = 2,
    stl= 3,
    ldnl= 4,
    stnl= 5,
    add= 6,
    sub = 7,
    shl= 8,
    shr= 9,
    adj= 10,
    a2sp= 11,
    sp2a = 12,
    call= 13,
    ret= 14,
    brz= 15,
    brlz= 16,
    br= 17,
    HALT=18 // Default case for invalid opcodes
};

void executeOpcode(int opcode, int operand) {
    switch(opcode) {
        case ldc: 
            // Load operand into regA and save old value of regA in regB
            regB = regA;
            regA = operand;
            break;
        
        case adc: 
            // Add operand to regA
            regA += operand;
            break;
        
        case ldl: 
            // Load value from mainMemory[SP + operand] into regA and save old value of regA in regB
            regB = regA;
            if (SP + operand >= 0 && SP + operand < memory.size()) {
                regA = memory[SP + operand];
            } else {
                cout << "Memory access error at SP + operand. Aborting.";
                exit(1);  // Handle out-of-bounds memory access error
            }
            break;
        
        case stl: 
            // Store value from regA to mainMemory[SP + operand] and restore regA to regB
            if (SP + operand >= 0 && SP + operand < memory.size()) {
                memory[SP + operand] = regA;
            } else {
                cout << "Memory access error at SP + operand. Aborting.";
                exit(1);  // Handle out-of-bounds memory access error
            }
            regA = regB;
            break;
        
        case ldnl: 
            // Load value from mainMemory[regA + operand] into regA
            if (regA + operand >= 0 && regA + operand < memory.size()) {
                regA = memory[regA + operand];
            } else {
                cout << "Memory access error at regA + operand. Aborting.";
                exit(1);  // Handle out-of-bounds memory access error
            }
            break;
        
        case stnl: 
            // Store value from regB to mainMemory[regA + operand]
            if (regA + operand >= 0 && regA + operand < memory.size()) {
                memory[regA + operand] = regB;
            } else {
                cout << "Memory access error at regA + operand. Aborting.";
                exit(1);  // Handle out-of-bounds memory access error
            }
            break;
        
        case add: 
            // Add regA and regB and store the result in regA
            regA = regB + regA;
            break;
        
        case sub: 
            // Subtract regA from regB and store the result in regA
            regA = regB - regA;
            break;
        
        case shl: 
            // Shift regB left by regA positions
            regA = regB << regA;
            break;
        
        case shr: 
            // Shift regB right by regA positions
            regA = regB >> regA;
            break;
        
        case adj: 
            // Add operand to SP (Stack Pointer)
            SP = SP + operand;
            break;
        
        case a2sp: 
            // Move value from SP to regA and regB to regA
            SP = regA;
            regA = regB;
            break;
        
        case sp2a: 
            // Move value from SP to regA and regB to regA
            regB = regA;
            regA = SP;
            break;
        
        case call: 
            // Save PC to regA and load operand-1 to PC
            regB = regA;
            regA = PC;
            PC = operand - 1;
            break;
        
        case ret: 
            // Save regA to PC and regB to regA
            PC = regA;
            regA = regB;
            break;
        
        case brz: 
            // Conditional jump: if regA is 0, update PC with operand
            if (regA == 0) {
                PC = PC + operand;
            }
            break;
        
        case brlz: 
            // Conditional jump: if regA is negative, update PC with operand
            if (regA < 0) {
                PC = PC + operand;
            }
            break;
        
        case br: 
            // Unconditional jump: update PC with operand
            PC = PC + operand;
            break;
        
        default: 
            // Handle invalid opcode
            cout << "Invalid opcode. Incorrect machine code. Aborting." << endl;
            exit(1);  // Exit program with error code
    }    
}

int argumentrun() {
    // Check if PC is within the bounds of objectFile size
    if (PC >= objectFile.size()) {
        cout << "Segmentation fault. Aborting.\n";
        exit(0);  // Exit if the PC exceeds the objectFile size
    }

    // Extract opcode and operand
    int opcode = objectFile[PC] & 0xFF;      // Last 8 bits (opcode)
    int operand = objectFile[PC] >> 8;       // First 24 bits (operand)

    // Print the mnemonic and operand in a formatted way
    cout << mnemonics[opcode] << "\t";
    printf("%08X\n", operand);

    // Handle HALT condition (opcode 18)
    if (opcode == 18) {
        total++;
        return 0;  // HALT, exit the function and return to the main function
    }

    // Execute the corresponding opcode with its operand
    executeOpcode(opcode, operand);

    // Increment total instructions executed and PC
    total++;
    PC++;

    // Stack overflow check
    if (SP > stackLimit) {
        cout << "Stack overflow. Aborting.\n";
        exit(0);  // Exit if stack pointer exceeds the stack limit
    }

    return 1;  // Return to indicate successful execution
}

pair<long, bool> read_operand(const std::string &operand) {
    if (operand.empty()) {
        return {0, false};  // Return default pair if operand is empty
    }
    char *end;
    long num;
    // Try to parse as a decimal
    num = std::strtol(operand.c_str(), &end, 10);
    if (*end == '\0') {  // Successful decimal conversion
        return {num, true};
    }
    // If decimal parsing fails, try parsing as hexadecimal
    num = std::strtol(operand.c_str(), &end, 16);
    if (*end == '\0') {  // Successful hexadecimal conversion
        return {num, true};
    }
    // If both conversions fail, return invalid result
    return {-1, false};
}

void dump() {
    std::string operand, offset;
    
    // Prompt for and read the base address
    std::cout << "Base address: ";
    std::cin >> operand;
    auto baseAddressResult = read_operand(operand);
    if (!baseAddressResult.second) {
        std::cerr << "Invalid base address input. Aborting.\n";
        return;
    }
    int baseAddress = baseAddressResult.first;

    // Prompt for and read the number of values
    std::cout << "No. of values: ";
    std::cin >> offset;
    auto offsetResult = read_operand(offset);
    if (!offsetResult.second) {
        std::cerr << "Invalid offset input. Aborting.\n";
        return;
    }
    int numValues = offsetResult.first;
    // Output memory content in blocks of 4
    for (int i = baseAddress; i < baseAddress + numValues; i += 4) {
        if (i + 3 >= (int)memory.size()) {
            std::cerr << "Memory access out of bounds at address " << i << ". Aborting.\n";
            return;
        }
        printf("%08X %08X %08X %08X %08X\n", i, memory[i], memory[i + 1], memory[i + 2], memory[i + 3]);
    }
}
int advance() {
    std::string temp;
    std::cout << "Emulator input: ";
    std::cin >> temp;
    // Convert input to lowercase for case-insensitivity
    std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);

    if (temp == "-t") {
        // Single-step execution with register status printout
        if (argumentrun()) {
            printf("A = %08X, B = %08X, PC = %08X, SP = %08X\n", regA, regB, PC, SP);
            return 1;  // Continue execution
        }
        return 0;  // End of execution
    } 
    else if (temp == "-all") {
        // Full execution until a stopping condition
        while (argumentrun()) {
            printf("A = %08X, B = %08X, PC = %08X, SP = %08X\n", regA, regB, PC, SP);
        }
        return 0;
    } 
    else if (temp == "-dump") {
        // Dump memory contents
        dump();
        return 1;  // Return to prompt for next command
    } 
    else {
        // Invalid input handling
        std::cerr << "Invalid emulator input" << std::endl;
        return 1;
    }
}
int main(int argc, char* argv[]) {
    std::string machineCodeFile = (argc > 1) ? argv[1] : "machineCode_t5.O";
    int tempData;

    // Attempt to open the specified machine code file
    std::ifstream currFile(machineCodeFile, std::ios::in | std::ios::binary);
    if (!currFile) {
        std::cerr << "Error opening file: " << machineCodeFile << std::endl;
        return 1;
    }

    // Read the binary file into the objectFile vector
    while (currFile.read(reinterpret_cast<char*>(&tempData), sizeof(int))) {
        objectFile.push_back(tempData);
    }
    currFile.close();

    // Load objectFile data into mainMemory
    for (size_t i = 0; i < objectFile.size(); ++i) {
        memory[i] = objectFile[i];
    }

    // Display user instructions
    std::cout << "Commands:\n"
              << "-t for trace\n"
              << "-dump for memory dump\n"
              << "-all for executing all commands\n"
              << "Enter commands with hyphen:\n";

    // Emulator input loop
    while (advance()) {
        // Continue prompting until advance() returns 0
    }
    std::cout << "Total instructions executed: " << total << std::endl;
    return 0;
}