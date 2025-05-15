#include <array>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace {

	std::condition_variable storeHasSpace;
	std::condition_variable storeHasData;

	constexpr std::streamsize BLOCK_SIZE = 1'000'000;
	constexpr std::streamsize BLOCK_NUM = 3;

	struct Block {
		char data[1024 * 1024];
		std::streamsize size;
	};

	auto blocks = std::make_unique< std::array<Block, BLOCK_NUM>>();

	std::queue<Block*> empty_;
	std::mutex emptyMtx_;
	std::queue<Block*> toWrite_;
	std::mutex toWriteMtx_;

	std::condition_variable waitLoad;
	std::condition_variable waitWrite;

	void reader(std::ifstream& input, std::queue<Block*>& emptyBlocks, std::queue<Block*>& toWrite)
	{
		bool loadFinished{ false };
		do {
			Block* block{};
			{
				std::unique_lock<std::mutex> lk(emptyMtx_);
				if (emptyBlocks.empty())
				{
					waitLoad.wait(lk, [&] { return !emptyBlocks.empty(); });
				}
				block = emptyBlocks.front();
				emptyBlocks.pop();
			}
			std::cout << "Loading a block.\n";
			input.read(block->data, BLOCK_SIZE);
			block->size = input.gcount();
			std::cout << "Loaded " << block->size << " bytes.\n";
			loadFinished = block->size != BLOCK_SIZE;
			{
				std::unique_lock<std::mutex> lk(toWriteMtx_);
				toWrite.push(block);
			}
			waitWrite.notify_one();

		} while (!loadFinished);
	}

	void writer(std::ofstream& output, std::queue<Block*>& emptyBlocks, std::queue<Block*>& toWrite)
	{
		bool writeFinished {false};
		do {
			Block* block{};
			{
				std::unique_lock lk(toWriteMtx_);
				if (toWrite.empty())
				{
					waitWrite.wait(lk, [&] { return !toWrite.empty(); });
				}
				block = toWrite.front();
				toWrite.pop();
			}
			std::cout << "Writing a block.\n";
			output.write(block->data, block->size);
			std::cout << "Wrote " << block->size << " bytes.\n";
			writeFinished = block->size != BLOCK_SIZE;
			{
				std::unique_lock lk(emptyMtx_);
				emptyBlocks.push(block);
			}
		} while (!writeFinished);
	}
} // namespace

int main(const int argc, const char* const argv[]) {
	try {
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

		for (auto& block : *blocks)
		{
			empty_.push(&block);
		}

		std::thread readerThread(reader, std::ref(inputFile), std::ref(empty_), std::ref(toWrite_));
		writer(outputFile, empty_, toWrite_);

		inputFile.close();
		outputFile.close();
		readerThread.join();
		std::cout << "File copying completed successfully.\n";

		return 0;
	}
	catch (const std::exception& e) {
		std::cout << "Error: " << e.what();
	}
}
