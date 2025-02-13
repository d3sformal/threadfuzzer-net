#pragma once
#include "net_types.hpp"

#include <cor.h>

class cor_profiler;

class cli_metadata_parser
{
    cor_profiler* profiler;
    IMetaDataImport2* metadata_import;
public:
    cli_metadata_parser(cor_profiler* profiler, IMetaDataImport2* metadata_import)
        : profiler(profiler), metadata_import(metadata_import)
    {
    }

    std::vector<const class_description*> parse_method_signature(PCCOR_SIGNATURE sig_blob, ULONG sig_size, const std::wstring& method_name);
    const class_description* parse_type(PCCOR_SIGNATURE sig_blob, ULONG sig_size);

private:
    std::vector<const class_description*> parse_method_signature(PCCOR_SIGNATURE sig_blob, ULONG& current_byte);

    const class_description* parse_next_type(PCCOR_SIGNATURE sigBlob, ULONG& current_byte);
};
