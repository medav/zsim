/** $lic$
 * Copyright (C) 2012-2015 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "prefetcher.h"
#include "bithacks.h"

//#define DBG(args...) info(args)
#define DBG(args...)

void StreamPrefetcher::setParents(uint32_t _childId, const g_vector<MemObject*>& parents, Network* network) {
    childId = _childId;
    if (parents.size() != 1) panic("Must have one parent");
    if (network) panic("Network not handled");
    parent = parents[0];
}

void StreamPrefetcher::setChildren(const g_vector<BaseCache*>& children, Network* network) {
    if (children.size() != 1) panic("Must have one children");
    if (network) panic("Network not handled");
    child = children[0];
}

void StreamPrefetcher::initStats(AggregateStat* parentStat) {
    AggregateStat* s = new AggregateStat();
    s->init(name.c_str(), "Prefetcher stats");
    profAccesses.init("acc", "Accesses"); s->append(&profAccesses);
    profPrefetches.init("pf", "Issued prefetches"); s->append(&profPrefetches);
    profDoublePrefetches.init("dpf", "Issued double prefetches"); s->append(&profDoublePrefetches);
    profPageHits.init("pghit", "Page/entry hit"); s->append(&profPageHits);
    profHits.init("hit", "Prefetch buffer hits, short and full"); s->append(&profHits);
    profShortHits.init("shortHit", "Prefetch buffer short hits"); s->append(&profShortHits);
    profStrideSwitches.init("strideSwitches", "Predicted stride switches"); s->append(&profStrideSwitches);
    profLowConfAccs.init("lcAccs", "Low-confidence accesses with no prefetches"); s->append(&profLowConfAccs);
    parentStat->append(s);
}

uint64_t StreamPrefetcher::access(MemReq& req) {
    uint32_t origChildId = req.childId;
    req.childId = childId;

    if (req.type != GETS) return parent->access(req); //other reqs ignored, including stores

    profAccesses.inc();

    uint64_t reqCycle = req.cycle;
    uint64_t respCycle = 0; //parent->access(req);

    Address pageAddr = req.lineAddr >> 6;
    uint32_t pos = req.lineAddr & (64-1);
    uint32_t idx = 16;
    // This loop gets unrolled and there are no control dependences. Way faster than a break (but should watch for the avoidable loop-carried dep)
    for (uint32_t i = 0; i < 16; i++) {
        bool match = (pageAddr == tag[i]);
        idx = match?  i : idx;  // ccmov, no branch
    }

    DBG("%s: 0x%lx page %lx pos %d", name.c_str(), req.lineAddr, pageAddr, pos);

    MESIState req_state = *req.state;

    auto block = req.lineAddr & ~0x3F;

    if (block == curBlock) {
        return respCycle;
    }

    curBlock = block;

    for (uint32_t i = 0; i < 64; i++) {
    MESIState state = I;

    auto lineAddr = curBlock + i; 

    if (lineAddr == req.lineAddr) continue;

    MemReq pfReq = {
        lineAddr,
        GETS,
        req.childId,
        &state,
        reqCycle,
        req.childLock,
        state,
        req.srcId,
        MemReq::PREFETCH
    };

    pfRespCycle = parent->access(pfReq);
    respCycle = (respCycle > pfRespCycle) ? respCycle : pfRespCycle;

    }


    profPrefetches.inc();


    uint64_t demand_resp = parent->access(req);
    respCycle = MAX(respCycle, demand_resp);    

    //newEv = new(evRec) StreamPrefetcherEvent(0, longerCycle, evRec);
    //nla = { pfReq.lineAddr, longerCycle, longerCycle, pfReq.type, newEv, newEv};

    //if (wbAcc.isValid())
    //        newEv->setAccessRecord(wbAcc, pfReq.cycle);

    //if (evRec && evRec->hasRecord()) {
    //        FirstFetchRecord = evRec->popRecord();
    //        newEv->setNextFetchRecord(FirstFetchRecord, pfReq.cycle);
    //}

    //evRec->pushRecord(nla);
    //req.childId = origChildId;
    return respCycle;

}

// nop for now; do we need to invalidate our own state?
uint64_t StreamPrefetcher::invalidate(const InvReq& req) {
    return child->invalidate(req);
}


