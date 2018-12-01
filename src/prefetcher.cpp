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

#include "bithacks.h"
#include "event_recorder.h"
#include "prefetcher.h"
#include "timing_event.h"
#include "zsim.h"
#include "math.h"

#define DBG(args...) info(args)
//#define DBG(args...)

void StreamPrefetcher::setParents(uint32_t _childId, const g_vector < MemObject * >&parents, Network * network)
{
    childId = _childId;
    if (parents.size() != 1)
        panic("Must have one parent");
    if (network)
        panic("Network not handled");
    parent = parents[0];
}

void StreamPrefetcher::setChildren(const g_vector < BaseCache * >&children, Network * network)
{
    if (children.size() < 1)
        panic("Must have one children");
    if (network)
        panic("Network not handled");
    child = children[0];
}

void StreamPrefetcher::initStats(AggregateStat * parentStat)
{
    AggregateStat *s = new AggregateStat();
    s->init(name.c_str(), "Prefetcher stats");
    profAccesses.init("acc", "Accesses");
    s->append(&profAccesses);
    profPrefetches.init("pf", "Issued prefetches");
    s->append(&profPrefetches);
    profDoublePrefetches.init("dpf", "Issued double prefetches");
    s->append(&profDoublePrefetches);
    profPageHits.init("pghit", "Page/entry hit");
    s->append(&profPageHits);
    profHits.init("hit", "Prefetch buffer hits, short and full");
    s->append(&profHits);
    profShortHits.init("shortHit", "Prefetch buffer short hits");
    s->append(&profShortHits);
    profStrideSwitches.init("strideSwitches", "Predicted stride switches");
    s->append(&profStrideSwitches);
    profLowConfAccs.init("lcAccs", "Low-confidence accesses with no prefetches");
    s->append(&profLowConfAccs);
    parentStat->append(s);
}

uint64_t StreamPrefetcher::access(MemReq & req)
{
    uint64_t longerCycle, pfRespCycle, respCycle, reqCycle;
    uint32_t origChildId = req.childId;
    int32_t fetchDepth;

    req.childId = childId;
    reqCycle = req.cycle;

    if (req.type != GETS) {
        respCycle = parent->access(req);
        req.childId = origChildId;
        return respCycle;       		//other reqs ignored, including stores
    }

    profAccesses.inc();

    EventRecorder *evRec = zinfo->eventRecorders[req.srcId];

    StreamPrefetcherEvent *newEv;
    TimingRecord nla, wbAcc, FirstFetchRecord, SecondFetchRecord;

    FirstFetchRecord.clear();
    SecondFetchRecord.clear();
    wbAcc.clear();

    longerCycle = pfRespCycle = respCycle = parent->access(req);

    if (likely(evRec && evRec->hasRecord())){
        wbAcc = evRec->popRecord();
    }

    Address pageAddr = req.lineAddr >> 6;
    uint32_t pos = req.lineAddr & (64-1);
    uint32_t idx = pfWays;

    //DBG("lineAddr %016X",req.lineAddr)
    // This loop gets unrolled and there are no control dependences. Way faster than a break (but should watch for the avoidable loop-carried dep)
    for (uint32_t i = 0; i < pfWays; i++) { 
        bool match = (pageAddr == tag[i][queTop[i]]); 
        idx = match?  i : idx;  // ccmov, no branch
        //DBG("idx %d pageAddr %016X tag[%d][%d] %016X",idx,pageAddr,i,queTop[i],tag[i][queTop[i]])
    }

    if (idx == pfWays) {  // entry miss
        uint32_t cand = pfWays;
        uint64_t candScore = -1;
        // check the head of each buffer
        for (uint32_t i = 0; i < pfWays; i++) {
            if (array[i][queBottom[i]].lastCycle > reqCycle + 500) continue;  // warm prefetches, not even a candidate //queBottom[i] gives the last access
            if (array[i][0].ts < candScore) {  // just LRU // just perform time-stamping at the head of each way
                cand = i;
                candScore = array[i][0].ts;
            }
        }

        if (cand < pfWays) {
            idx = cand;
            // reset queue pointers
            resetQueuePointers(idx);

            array[idx][queTop[idx]].alloc(reqCycle);
            array[idx][0].ts = timestamp++;
            tag[idx][queTop[idx]] = pageAddr + 1; // The top entry for prefetch becomes the subsequent line after Miss. 
            //start demand access on Miss

            MESIState state = I;
            MESIState req_state = *req.state;
            uint64_t nextLineAddr = (pageAddr+1) << 6;

            MemReq pfReq = {
                nextLineAddr,
                GETS,
                req.childId,
                &state,
                respCycle, // start after the demand fetch
                req.childLock,
                state,
                req.srcId,
                MemReq::PREFETCH
            };
            pfRespCycle = parent->access(pfReq); // update the access for next address
            array[idx][queBottom[idx]].time.fill(respCycle,pfRespCycle);
            array[idx][queBottom[idx]].lastCycle = pfRespCycle;
            array[idx][queBottom[idx]].valid = true;
            //DBG("MISSSS pageAddr %016X lineAddr %016X pfRespCycle %d queBottom[idx] %d",tag[idx][queBottom[idx]],nextLineAddr,pfRespCycle,queBottom[idx])
            queBottom[idx]++;       // points to the next available slot
        }
        //DBG("%s: MISS alloc idx %d", name.c_str(), idx);
    } else { // prefetch Hit
        if(array[idx][queTop[idx]].valid==true && (reqCycle > array[idx][queTop[idx]].time.respCycle)){ // check whether the prefetch data is valid or not
            DBG("VALID PREFETCH HIT %d %d %d",idx,reqCycle,array[idx][queTop[idx]].time.respCycle)
            //respCycle = array[idx][queTop[idx]].time.respCycle;
            profPageHits.inc();  
            numHits[idx]++;         // could be removed            
            // incremental prefetches
            uint32_t entriesOccupied = (queBottom[idx] > queTop[idx]) ? queBottom[idx]-queTop[idx] : queTop[idx]-queBottom[idx];
            uint32_t numPrefetchers = (pow(2,numHits[idx]) > (pfEntries-entriesOccupied)) ? (pfEntries-entriesOccupied) : pow(2,numHits[idx]);
            //DBG("%s: PAGE HIT idx %d Top Pointer %d numHits[%d] %d numPrefetchers %d", name.c_str(), idx, queTop[idx], idx, numHits[idx], numPrefetchers); //);

            for (uint8_t i=0; i<numPrefetchers; i++){
                if( queBottom[idx] != queTop[idx]) {
                    pageAddr++;
                    tag[idx][queBottom[idx]] = pageAddr; // The top entry for prefetch becomes the subsequent line after Miss. 
                    array[idx][queBottom[idx]].ts = timestamp++; // Incrementing Time Stamps

                    MESIState state = I;
                    MESIState req_state = *req.state;
                    uint64_t nextLineAddr = pageAddr << 6;

                    MemReq pfReq = {
                        nextLineAddr,
                        GETS,
                        req.childId,
                        &state,
                        reqCycle,
                        req.childLock,
                        state,
                        req.srcId,
                        MemReq::PREFETCH
                    };
                    pfRespCycle = parent->access(pfReq); // update the access for next address
                    //DBG("pageAddr %016X lineAddr %016X pfRespCycle %d queBottom[idx] %d",tag[idx][queBottom[idx]],nextLineAddr,pfRespCycle,queBottom[idx])
                    queBottom[idx] = (queBottom[idx] > pfEntries-1) ? 0 : queBottom[idx] + 1;       // adding an entry to the designated buffer
                }
            }
            queTop[idx]++;          // increments the head of the queue on a Hit  
        }else DBG("PREFETCH NOT READY %d %d %d",idx,reqCycle,array[idx][queTop[idx]].time.respCycle)
    }

    // since this was updated in the processAccess call
    //if (wbAcc.isValid())
    //        evRec->pushRecord(wbAcc);

    req.childId = origChildId;

    return respCycle;
}

// nop for now; do we need to invalidate our own state?
uint64_t StreamPrefetcher::invalidate(const InvReq& req) {
    return child->invalidate(req);
}
