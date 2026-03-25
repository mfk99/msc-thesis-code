#include <string>
#include "../../../libs/rustsat/capi/rustsat.h"
#include "am1_encoder.h"

using namespace std;
using namespace RustSAT;

AM1Encoder::AM1Encoder(string encoderType)
{
    if (encoderType == "bimander")
    {
        encoder = bimander_new();
        add_fn = [](void *encoder, int lit)
        { bimander_add((Bimander *)encoder, lit); };
        encode_fn = [](void *bimander, uint32_t *n_vars_used, RustSAT::CClauseCollector collector, void *collector_data)
        { bimander_encode((Bimander *)bimander, n_vars_used, collector, collector_data); };
        drop_fn = [](void *encoder)
        { bimander_drop((Bimander *)encoder); };
    }

    if (encoderType == "bitwise")
    {
        encoder = bitwise_new();
        add_fn = [](void *encoder, int lit)
        { bitwise_add((Bitwise *)encoder, lit); };
        encode_fn = [](void *bitwise, uint32_t *n_vars_used, RustSAT::CClauseCollector collector, void *collector_data)
        { bitwise_encode((Bitwise *)bitwise, n_vars_used, collector, collector_data); };
        drop_fn = [](void *encoder)
        { bitwise_drop((Bitwise *)encoder); };
    }

    if (encoderType == "commander")
    {
        encoder = commander_new();
        add_fn = [](void *encoder, int lit)
        { commander_add((Commander *)encoder, lit); };
        encode_fn = [](void *commander, uint32_t *n_vars_used, RustSAT::CClauseCollector collector, void *collector_data)
        { commander_encode((Commander *)commander, n_vars_used, collector, collector_data); };
        drop_fn = [](void *encoder)
        { commander_drop((Commander *)encoder); };
    }

    if (encoderType == "ladder")
    {
        encoder = ladder_new();
        add_fn = [](void *encoder, int lit)
        { ladder_add((Ladder *)encoder, lit); };
        encode_fn = [](void *ladder, uint32_t *n_vars_used, RustSAT::CClauseCollector collector, void *collector_data)
        { ladder_encode((Ladder *)ladder, n_vars_used, collector, collector_data); };
        drop_fn = [](void *encoder)
        { ladder_drop((Ladder *)encoder); };
    }

    if (encoderType == "pairwise")
    {
        encoder = pairwise_new();
        add_fn = [](void *encoder, int lit)
        { pairwise_add((Pairwise *)encoder, lit); };
        encode_fn = [](void *pairwise, uint32_t *n_vars_used, RustSAT::CClauseCollector collector, void *collector_data)
        { pairwise_encode((Pairwise *)pairwise, n_vars_used, collector, collector_data); };
        drop_fn = [](void *encoder)
        { pairwise_drop((Pairwise *)encoder); };
    }
}

void AM1Encoder::am1encoder_add(int lit)
{
    add_fn(encoder, lit);
}

void AM1Encoder::am1encoder_encode(uint32_t *n_vars_used, RustSAT::CClauseCollector collector, void *collector_data)
{
    encode_fn(encoder, n_vars_used, collector, collector_data);
}

void AM1Encoder::am1encoder_drop()
{
    drop_fn(encoder);
}
