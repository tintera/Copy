#include <fstream>
#include <iostream>
#include <string>
#include <array>
#include <thread>
#include <mutex>
#include <condition_variable>

import limited_queue;

namespace {

std::mutex mtx;
std::condition_variable bufferHasSpace;
std::condition_variable bufferHasData;

constexpr size_t BUFFER_SIZE = 4096; // 4KB
constexpr size_t MAX_BLOCKS = 50;
using block = std::array<char, BUFFER_SIZE>;

struct buffer
{
public:
    LimitedQueue<block> chunks{MAX_BLOCKS};
    std::streamsize lastBlockSize = BUFFER_SIZE;
} inter;


void reader(std::ifstream &input, buffer& store)
{
    std::streamsize bytesRead = 0;
    do{
        std::array<char, BUFFER_SIZE> buffer;
        input.read(buffer.data(), sizeof(buffer));
        bytesRead = input.gcount();
        {
            std::unique_lock<std::mutex> lk(mtx);
            if (store.chunks.full())
            {
                bufferHasSpace.wait(lk, [&store]() { return !store.chunks.full(); });
            }
            store.chunks.push(buffer);
            store.lastBlockSize = bytesRead;
            bufferHasData.notify_one();
        }
        std::cout << store.chunks.size() << "r ";
    } while (bytesRead == BUFFER_SIZE);
    std::cout << store.lastBlockSize << " lastBlockSize ";

}

void writter(std::ofstream& output, buffer& store)
{
    do {
        std::unique_lock<std::mutex> lk(mtx);
        bufferHasData.wait(lk, [&store]()
            {
                return !store.chunks.empty();
            });
        std::array<char, BUFFER_SIZE> chunk = store.chunks.front();
        std::streamsize amount = (store.chunks.size() == 1) ? store.lastBlockSize : BUFFER_SIZE;
        store.chunks.pop();
        lk.unlock();
        bufferHasSpace.notify_one();
        output.write(chunk.data(), amount);
        std::cout << store.chunks.size() << "w ";
    } while(!store.chunks.empty() || (store.lastBlockSize == BUFFER_SIZE));
}
} // namespace

int main(int argc, char* argv[]) {
    try{
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " <input_file> <output_file>\n";
            return 1;
        }
        const char* inputFileName = argv[1];
        const char* outputFileName = argv[2];

        std::ios::sync_with_stdio(false);
        std::ifstream inputFile(inputFileName, std::ios::binary);
        if (!inputFile.is_open()) {
            std::cerr << "Error: Could not open input file " << inputFileName << "\n";
            return 1;
        }

        std::ofstream outputFile(outputFileName, std::ios::binary);
        if (!outputFile.is_open()) {
            std::cerr << "Error: Could not open output file " << outputFileName << "\n";
            inputFile.close();
            return 1;
        }

        std::thread readerThread(reader, std::ref(inputFile), std::ref(inter));
        writter(outputFile, inter);

        //std::string line;
        //while (std::getline(inputFile, line)) {
        //    outputFile << line << "\n";
        //}

        inputFile.close();
        outputFile.close();
        readerThread.join();
        std::cout << "File copying completed successfully.\n";

        return 0;
    } catch (const std::exception& e){
        std::cout << "Error: " << e.what();
    }
}
