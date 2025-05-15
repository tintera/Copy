#include <fstream>
#include <iostream>
#include <string>
#include <array>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace {

	std::mutex mtx;
	std::condition_variable storeHasSpace;
	std::condition_variable storeHasData;

	constexpr size_t BLOCK_SIZE = 1'000'000; // 4KB
	constexpr std::int8_t NUM_BLOCKS = 3;
	using block = std::array<char, BLOCK_SIZE>;

	struct altBlock {
		char data[1024 * 1024];
		size_t size;
	};


	struct cache_t
	{
	private:
		//std::mutex mtx;

		std::int8_t nextLoadIdx_() {
			return (loaded + 1) % NUM_BLOCKS;
		}

		std::int8_t nextWriteIdx_() {
			return (written + 1) % NUM_BLOCKS;
		}

		std::streamsize outBlockSize_(std::int8_t writeIdx) {
			return LoadFinished() && (loaded == writeIdx) ? lastBlockSize : BLOCK_SIZE;
		}

		bool LoadFinished() {
			return (lastBlockSize != BLOCK_SIZE);
		}

		bool WriteFinished() {
			return LoadFinished() && (written == loaded);
		}

		std::condition_variable waitLoad;
		std::condition_variable waitWrite;

	public:
		std::streamsize lastBlockSize = BLOCK_SIZE;
		std::array <block, NUM_BLOCKS> cache;

		char* getInputPtr(std::unique_lock<std::mutex>& lk)
		{
			if (isFull()) {
				storeHasSpace.wait(lk, [this]() { return !isFull(); });
			}
			return &(cache[nextLoadIdx_()][0]);
		}

		auto getOutputPtr(std::unique_lock<std::mutex>& lk)
		{
			if (isEmpty()) {
				storeHasData.wait(lk, [this]() { return !isEmpty(); });
			}
			auto nextIdx = nextWriteIdx_();
			return std::tuple<char*, std::streamsize>{ &(cache[nextIdx][0]), outBlockSize_(nextIdx) };
		}

		// -1 nothing was done or index to an array last done.
		std::int8_t loaded = -1;
		std::int8_t written = -1;

		bool isEmpty() {
			return (loaded == -1) || (written == loaded);
		}

		bool isFull() {
			return (written == nextLoadIdx_()) || (written == -1 && loaded == (NUM_BLOCKS - 1));
		}

		// One buffer has been loaded
		bool blockLoadDone(std::streamsize bytesLoaded) {
			lastBlockSize = bytesLoaded;
			loaded = nextLoadIdx_();
			return LoadFinished();
		}

		// One buffer was written
		bool blockWriteDone() {
			written = nextWriteIdx_();
			return WriteFinished();
		}
	};

	std::unique_ptr<cache_t> buffers = std::make_unique<cache_t>();


	void reader(std::ifstream& input, cache_t& store)
	{
		bool loadFinished{ false };
		std::int8_t where = 0;
		do {
			char* inputPtr;
			{
				std::unique_lock<std::mutex> lk(mtx);
				inputPtr = store.getInputPtr(lk);
			}
			input.read(inputPtr, BLOCK_SIZE);
			{
				std::unique_lock<std::mutex> lk(mtx);
				loadFinished = store.blockLoadDone(input.gcount());
			}
			storeHasData.notify_one();
		} while (!loadFinished);
		std::cout << " lastBlockSize: " << store.lastBlockSize << "\n";
	}

	void writter(std::ofstream& output, cache_t& store)
	{
		bool writeFinished{ false };
		do {
			std::unique_lock<std::mutex> lk(mtx);
			auto [outPtr, amount] = store.getOutputPtr(lk);
			lk.unlock();
			output.write(outPtr, amount);
			lk.lock();
			writeFinished = store.blockWriteDone();
			lk.unlock();
			storeHasSpace.notify_one();
		} while (!writeFinished);
	}
} // namespace

int main(int argc, char* argv[]) {
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

		std::thread readerThread(reader, std::ref(inputFile), std::ref(*buffers));
		writter(outputFile, *buffers);

		//std::string line;
		//while (std::getline(inputFile, line)) {
		//    outputFile << line << "\n";
		//}

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
