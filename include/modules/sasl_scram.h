// Anope IRC Services <https://www.anope.org/>
//
// SPDX-License-Identifier: GPL-2.0-only
//
// Shared interface for third-party SCRAM SASL modules.

#pragma once

#include "service.h"

class NickCore;

namespace SASLScram
{
        class VerifierService
                : public Service
        {
        public:
                VerifierService(Module *creator, const Anope::string &name)
                        : Service(creator, "SASLScram::VerifierService", name)
                {
                }

                virtual bool HasVerifier(NickCore *nc) const = 0;
                virtual void SetVerifierFromPassword(NickCore *nc, const Anope::string &password) = 0;
        };
}