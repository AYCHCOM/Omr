/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#ifndef OMR_CFG_INCL
#define OMR_CFG_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CFG_CONNECTOR
#define OMR_CFG_CONNECTOR
namespace OMR { class CFG; }
namespace OMR { typedef OMR::CFG CFGConnector; }
#endif

#include <stddef.h>                 // for NULL
#include <stdint.h>                 // for uint32_t
#include "compile/Compilation.hpp"  // for Compilation
#include "cs2/tableof.h"            // for TableOf
#include "cs2/listof.h"             // for ListOf
#include "env/TRMemory.hpp"         // for Allocator, TR_Memory, etc
#include "il/Node.hpp"              // for vcount_t
#include "infra/Assert.hpp"         // for TR_ASSERT
#include "infra/List.hpp"           // for TR_TwoListIterator, List
#include "infra/Link.hpp"           // for TR_LinkHead1
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"

class TR_RegionStructure;
class TR_Structure;
class TR_StructureSubGraphNode;
class TR_BitVector;
class TR_BlockCloner;
class TR_BlockFrequencyInfo;
class TR_ExternalProfiler;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace TR { class Compilation; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;


#define MAX_STATIC_EDGE_FREQ 15
#define INITIAL_BLOCK_FREQUENCY_FACTOR 20
#define MAX_REGION_FACTOR 3500.0f

#define MAX_BLOCK_COUNT 9995
#define UNKNOWN_COLD_BLOCK_COUNT 0
#define VERSIONED_COLD_BLOCK_COUNT 1
#define UNRESOLVED_COLD_BLOCK_COUNT 2
#define CATCH_COLD_BLOCK_COUNT 3
#define INTERP_CALLEE_COLD_BLOCK_COUNT 4
#define REVERSE_ARRAYCOPY_COLD_BLOCK_COUNT 5
#define MAX_COLD_BLOCK_COUNT 5

#define MAX_WARM_BLOCK_COUNT ((MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT)/10)
#define MAX_HOT_BLOCK_COUNT (2*MAX_WARM_BLOCK_COUNT)
#define MAX_VERY_HOT_BLOCK_COUNT (3*MAX_WARM_BLOCK_COUNT)
#define MAX_SCORCHING_BLOCK_COUNT (5*MAX_WARM_BLOCK_COUNT)

#define MAX_REMOVE_EDGE_NESTING_DEPTH 125

#define LOW_FREQ 5
#define AVG_FREQ 150


namespace OMR
{

bool alwaysTrue(TR::CFGEdge * e);

class CFG
   {
   public:

   TR_ALLOC(TR_Memory::CFG)

   CFG(TR::Compilation *c, TR::ResolvedMethodSymbol *m) :
      _compilation(c),
      _method(m),
      _rootStructure(NULL),
      _pStart(NULL),
      _pEnd(NULL),
      _nextNodeNumber(0),
      _numEdges(0),
      _mightHaveUnreachableBlocks(false),
      _doesHaveUnreachableBlocks(false),
      _removingUnreachableBlocks(false),
      _ignoreUnreachableBlocks(false),
      _structureGraphNodes(c->allocator()),
      _removeEdgeNestingDepth(0),
      _forwardTraversalOrder(NULL),
      _backwardTraversalOrder(NULL),
      _forwardTraversalLength(0),
      _backwardTraversalLength(0),
      _maxFrequency(-1),
      _maxEdgeFrequency(-1),
      _oldMaxFrequency(-1),
      _oldMaxEdgeFrequency(-1),
      _frequencySet(NULL),
      _calledFrequency(0),
      _initialBlockFrequency(-1),
      _edgeProbabilities(NULL)
      {
      }

   TR::CFG * self();

   TR::Compilation *comp() { return _compilation; }
   TR::ResolvedMethodSymbol *getMethodSymbol() {return _method; }

   TR_Memory *trMemory() { return comp()->trMemory(); }
   TR_HeapMemory trHeapMemory() { return trMemory(); }
   TR_StackMemory trStackMemory() { return trMemory(); }

   void setStartAndEnd(TR::CFGNode * s, TR::CFGNode * e) { addNode(s); addNode(e); setStart(s); setEnd(e); }

   TR::CFGNode *getStart() {return _pStart;}
   TR::CFGNode *setStart(TR::CFGNode *p) {return (_pStart = p);}

   TR::CFGNode *getEnd() {return _pEnd;}
   TR::CFGNode *setEnd(TR::CFGNode *p) {return (_pEnd = p);}

   TR_Structure *getStructure() {return _rootStructure;}
   TR_Structure *setStructure(TR_Structure *p);
   TR_Structure *invalidateStructure();

   TR::CFGNode *getFirstNode() {return _nodes.getFirst();}
   TR_LinkHead1<TR::CFGNode> & getNodes() {return _nodes;}

   int32_t getNumberOfNodes() {return _nodes.getSize();}
   int32_t getNextNodeNumber() {return _nextNodeNumber;}
   void setNextNodeNumber(int32_t n) {_nextNodeNumber = n;}
   int32_t allocateNodeNumber() {return _nextNodeNumber++;}

   TR::CFGNode *addNode(TR::CFGNode *n, TR_RegionStructure *parent = NULL, bool isEntryInParent = false);

   uint32_t addStructureSubGraphNodes (TR_StructureSubGraphNode *node);
   void removeStructureSubGraphNodes(TR_StructureSubGraphNode *node);
   TR_StructureSubGraphNode *getStructureSubGraphNode (uint32_t index) {return _structureGraphNodes[index];}

   void addEdge(TR::CFGEdge *e);
   TR::CFGEdge *addEdge(TR::CFGNode *f, TR::CFGNode *t, TR_AllocationKind = heapAlloc);
   void addExceptionEdge(TR::CFGNode *f, TR::CFGNode *t, TR_AllocationKind = heapAlloc);
   void addSuccessorEdges(TR::Block * block);

   void copyExceptionSuccessors(TR::CFGNode *from, TR::CFGNode *to, bool (*predicate)(TR::CFGEdge *) = OMR::alwaysTrue);

   bool getMightHaveUnreachableBlocks() { return _mightHaveUnreachableBlocks; }
   void setHasUnreachableBlocks() { _doesHaveUnreachableBlocks = true; }
   bool getHasUnreachableBlocks() { return _doesHaveUnreachableBlocks; }

   bool getIgnoreUnreachableBlocks() { return _ignoreUnreachableBlocks;}
   void setIgnoreUnreachableBlocks(bool flag) {_ignoreUnreachableBlocks = flag; }

   /// Connect the treetops between b1 and b2 and add the successor edges to b1.
   void join(TR::Block * b1, TR::Block * b2);

   /// Will add b1 to the cfg and call join between b1 and b2
   void insertBefore(TR::Block * b1, TR::Block * b2);


   void removeUnreachableBlocks();
   int32_t compareExceptionSuccessors(TR::CFGNode *, TR::CFGNode *);

   /**
    * Set up profiling frequencies for nodes and edges, normalized to the
    * maxBlockCount in TR::Recompilation.
    *
    * Returns true if profiling information was available and used.
    */
   bool setFrequencies();
   void resetFrequencies();

   TR::CFGNode *removeNode(TR::CFGNode *n);

   /// Remove an edge.
   ///
   /// \return true if blocks were removed as a result of removing
   /// the edge.
   bool removeEdge(TR::CFGEdge *e);
   bool removeEdge(TR::CFGNode *from, TR::CFGNode *to);
   bool removeEdge(TR::CFGEdge *e, bool recursiveImpl);
   void removeSelfEdge(List<TR::CFGEdge> succList, int32_t selfNumber);
   void removeEdge(TR::list<TR::CFGEdge*> succList, int32_t selfNumber, int32_t destNumber);

   void removeBlock(TR::Block *);

   void findReachableBlocks(TR_BitVector *result);

   /// reset the visit count on all nodes and edges
   void resetVisitCounts(vcount_t);

   TR::TreeTop * findLastTreeTop();
   TR::Block * * createArrayOfBlocks(TR_AllocationKind = stackAlloc);

   /// Find all blocks that are in loops
   void findLoopingBlocks(TR::BitVector&);

   int32_t createTraversalOrder(bool forward, TR_AllocationKind allocationKind, TR_BitVector *backEdges = NULL);
   TR::CFGNode *getForwardTraversalElement(int32_t idx) { return _forwardTraversalOrder[idx]; }
   int32_t getForwardTraversalLength() { return _forwardTraversalLength; }
   TR::CFGNode *getBackwardTraversalElement(int32_t idx) { return _backwardTraversalOrder[idx]; }
   int32_t getBackwardTraversalLength() { return _backwardTraversalLength; }

   TR_BlockCloner * clone();

   void propagateColdInfo(bool);


   bool consumePseudoRandomFrequencies();


   void setMaxFrequency(int32_t f) { _maxFrequency = f; }
   int32_t getMaxFrequency() { return _maxFrequency; }
   int32_t getLowFrequency();
   int32_t getAvgFrequency();

   void setMaxEdgeFrequency(int32_t f) { _maxEdgeFrequency = f; }
   int32_t getMaxEdgeFrequency() { return _maxEdgeFrequency; }

   int32_t getOldMaxEdgeFrequency() { return _oldMaxEdgeFrequency; }

   TR_BitVector *getFrequencySet() { return _frequencySet; }

   int32_t getInitialBlockFrequency() { return _initialBlockFrequency; }

   void propagateFrequencyInfoFrom(TR_Structure *str);
   void processAcyclicRegion(TR_RegionStructure *region);
   void processNaturalLoop(TR_RegionStructure *region);
   void walkStructure(TR_RegionStructure *region);
   bool setEdgeFrequenciesFrom();
   void computeEntryFactorsFrom(TR_Structure *str, float &factor);
   void computeEntryFactorsAcyclic(TR_RegionStructure *region);
   void computeEntryFactorsLoop(TR_RegionStructure *region);
   float computeOutsideEdgeFactor(TR::CFGEdge *outEdge, TR::CFGNode *pred);
   float computeInsideEdgeFactor(TR::CFGEdge *inEdge, TR::CFGNode *pred);
   void propagateEntryFactorsFrom(TR_Structure *str, float factor);

   void scaleEdgeFrequencies();

   void setBlockAndEdgeFrequenciesBasedOnStructure();
   void getBranchCounters (TR::Node *node, TR::Block *block, int32_t *taken, int32_t *notTaken, TR::Compilation *comp);

   void normalizeFrequencies(TR_BitVector *);

   void setEdgeFrequenciesOnNode(TR::CFGNode *node, int32_t branchToCount, int32_t fallThroughCount, TR::Compilation *comp);
   void setUniformEdgeFrequenciesOnNode(TR::CFGNode *node, int32_t branchToCount, bool addFrequency, TR::Compilation *comp);

   void setEdgeProbability(TR::CFGEdge *edge, double prob)
         {
         if (_edgeProbabilities)
           {
           TR_ASSERT(edge->getId() != -1, "Edge from %d to %d has id -1\n",
                                          edge->getFrom()->getNumber(),
                                          edge->getTo()->getNumber());

           _edgeProbabilities[edge->getId()] = prob;
           }
         }

   double getEdgeProbability(TR::CFGEdge *edge)
         {
         if (_edgeProbabilities)
            {
            TR_ASSERT(edge->getId() != -1, "Edge from %d to %d has id -1\n",
                                          edge->getFrom()->getNumber(),
                                          edge->getTo()->getNumber());
            return _edgeProbabilities[edge->getId()];
            }
         else
            return 0;
         }


   bool updateBlockFrequency(TR::Block *block, int32_t newFreq);
   void updateBlockFrequencyFromEdges(TR::Block *block);

   // Is branch profiling data available?
   //
   bool hasBranchProfilingData() { return false; }

   // Extract branch counter information from available branch profiling data
   //
   void getBranchCountersFromProfilingData(TR::Node *node, TR::Block *block, int32_t *taken, int32_t *notTaken) { return; }

protected:

   TR::Compilation *_compilation;
   TR::ResolvedMethodSymbol *_method;

   TR::CFGNode *_pStart;
   TR::CFGNode *_pEnd;
   TR_Structure *_rootStructure;

   TR_LinkHead1<TR::CFGNode> _nodes;
   CS2::TableOf <TR_StructureSubGraphNode *,TR::Allocator> _structureGraphNodes;
   int32_t _numEdges;
   int32_t _nextNodeNumber;

   bool _mightHaveUnreachableBlocks;
   bool _doesHaveUnreachableBlocks;
   bool _ignoreUnreachableBlocks;
   bool _removingUnreachableBlocks;


   TR::CFGNode **_forwardTraversalOrder;
   int32_t _forwardTraversalLength;

   TR::CFGNode **_backwardTraversalOrder;
   int32_t _backwardTraversalLength;

   void normalizeNodeFrequencies(TR_BitVector *,TR_Array<TR::CFGEdge *> *);
   void normalizeEdgeFrequencies(TR_Array<TR::CFGEdge *> *);

   int32_t                  _removeEdgeNestingDepth;

   int32_t                  _maxFrequency;
   int32_t                  _maxEdgeFrequency;
   int32_t                  _oldMaxFrequency;
   int32_t                  _oldMaxEdgeFrequency;
   TR_BitVector            *_frequencySet;
   double                  *_edgeProbabilities; // temp array

public: //FIXME: These public members should eventually be wrtapped in an interface.
   int32_t                  _max_edge_freq;
   int32_t                  _calledFrequency;
   int32_t                  _initialBlockFrequency;
   static const int32_t MAX_PROF_EDGE_FREQ=0x3FFE;
   enum
    {
     StartBlock = 0,
     EndBlock   = 1
    };
   };

}




/**
 * The following class is an API that can be used to iterate over multiple
 * CFGEdge lists.
 *
 * It is primarily used in the form of a TR_SuccessorIterator and
 * TR_PredecessorIterator
 */
class TR_CFGIterator
   {
public:
   TR_CFGIterator(TR::list<TR::CFGEdge*>& l1, TR::list<TR::CFGEdge*>& l2) : combinedList(l1)
      {
      combinedList.insert(combinedList.end(), l2.begin(), l2.end());
      currentIterator = combinedList.begin();
      }

   /**
    * Get the first Element of the combinedList, return NULL if empty
    */
   TR::CFGEdge* getFirst()
      {
      currentIterator = combinedList.begin();
      return (combinedList.empty()) ? NULL : *currentIterator ;
      }

   /**
    * Get the current Element that the iterator points to
    */
   TR::CFGEdge* getCurrent()
      {
      return (currentIterator == combinedList.end()) ? NULL : *currentIterator;
      }

   /**
    * Get the next element, return NULL if we've hit the end of the list
    */
   TR::CFGEdge* getNext()
      {
      return (++currentIterator == combinedList.end()) ? NULL : *currentIterator;
      }

private:
   TR::list<TR::CFGEdge*> combinedList;

   std::list<TR::CFGEdge*,
             TR::typed_allocator<TR::CFGEdge*,
             TR::Allocator>
            >::iterator currentIterator;
   };

class TR_SuccessorIterator : public TR_CFGIterator
   {
public:
   TR_SuccessorIterator(TR::CFGNode * node)
      : TR_CFGIterator(node->getSuccessors(), node->getExceptionSuccessors())
      {
      }
   };


class TR_PredecessorIterator : public TR_CFGIterator
   {
public:
   TR_PredecessorIterator(TR::CFGNode * node)
      : TR_CFGIterator(node->getPredecessors(), node->getExceptionPredecessors())
      {
      }
   };

class TR_OrderedExceptionHandlerIterator
   {
public:
   TR_ALLOC(TR_Memory::OrderedExceptionHandlerIterator)

   TR_OrderedExceptionHandlerIterator(TR::Block * tryBlock);

   TR::Block * getFirst();

   TR::Block * getNext();
private:

   TR::Block * getCurrent();

   TR::Block ** _handlers;
   uint32_t _index, _dim;
   };

#endif
