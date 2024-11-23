/*
 * Copyright 2016 Microsoft, Inc.
 * Modifications copyright (c) 2023 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

extern "C" {
#include "sai.h"
}

#include "swss/ipaddress.h"
#include "swss/ipprefix.h"

#include "vppxlate/SaiVppXlate.h"

namespace saivs
{
    sai_status_t find_attrib_in_list(
            _In_ uint32_t                       attr_count,
            _In_ const sai_attribute_t         *attr_list,
            _In_ sai_attr_id_t                  attrib_id,
            _Out_ const sai_attribute_value_t **attr_value,
            _Out_ uint32_t                     *index);

    int getPrefixLenFromAddrMask(const uint8_t *addr, int len);

    swss::IpPrefix getIpPrefixFromSaiPrefix(const sai_ip_prefix_t& src);

    sai_ip_prefix_t& subnet(sai_ip_prefix_t& dst, const sai_ip_prefix_t& src);

    sai_ip_prefix_t& copy(sai_ip_prefix_t& dst, const swss::IpPrefix& src);

    void sai_ip_address_t_to_vpp_ip_addr_t(sai_ip_address_t& src, vpp_ip_addr_t& dst);

    /* Utility function for IP addr translation from VS to SAI */
    void vpp_ip_addr_t_to_sai_ip_address_t(vpp_ip_addr_t& src, sai_ip_address_t& dst);
}
