#pragma once

#include <string>
#include "../../../../rustsat/capi/rustsat.h"

using namespace std;
using namespace RustSAT;

class AM1Encoder
{
private:
    void *encoder;
    void (*add_fn)(void *, int);
    void (*encode_fn)(void *, uint32_t *, RustSAT::CClauseCollector, void *);
    void (*drop_fn)(void *);

public:
    AM1Encoder(string encoderType);

    void am1encoder_add(int lit);

    void am1encoder_encode(uint32_t *n_vars_used, RustSAT::CClauseCollector collector, void *collector_data);

    void am1encoder_drop();
};