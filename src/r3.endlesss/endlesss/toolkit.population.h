//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include <tsl/htrie_set.h>
#include "absl/hash/internal/city.h"

struct HtrieCityHash
{
    std::size_t operator()( const char* key, std::size_t key_size ) const
    {
        return absl::hash_internal::CityHash64( key, key_size );
    }
};

namespace config { struct IPathProvider; }

namespace endlesss {
namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
struct PopulationQuery
{
    // limit the results returned to a fixed amount; re-use Result instances to avoid re-allocating string results
    // as the query decompresses name data from the trie
    static constexpr std::size_t    MaximumQueryResults = 8;
    struct Result
    {
        Result()
        {
            for ( auto& value : m_values )
                value.reserve( 48 );
        }

        void clear()
        {
            m_validCount = 0;
            for ( auto& value : m_values )
                value.clear();
        }

        std::size_t                                       m_validCount = 0;
        std::array< std::string, MaximumQueryResults >    m_values;
    };

    // fetch all usernames we know about into acceleration structures suitable for fast partial lookup
    // ideally do this on a thread
    void loadPopulationData( const config::IPathProvider& pathProvider );

    // run a prefix query on the loaded data, returning up to MaximumQueryResults matching results;
    // this finds all usernames that begin with the string fragment provided.
    // returns false if the lookup isn't built or there was no results found
    bool prefixQuery( const std::string_view prefix, Result& result ) const;


    ouro_nodiscard inline bool isValid() const { return m_nameTrieValid; }

private:

    tsl::htrie_set<char, HtrieCityHash>     m_nameTrie;
    std::atomic_bool                        m_nameTrieValid = false;
};

} // namespace toolkit
} // namespace endlesss
