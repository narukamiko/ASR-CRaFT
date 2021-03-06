/*
 * CRF_ViterbiDecoder.cpp
 *
 * Copyright (c) 2010
 * Author: Jeremy Morris
 *
 */
#include "CRF_ViterbiDecoder.h"
#include <cmath>
#include <vector>
#include <deque>
#include <map>


/*
 * CRF_ViterbiDecoder constructor
 *
 * Input: *ftr_strm_in - pointer to input feature stream for decoding
 *        *crf_in - pointer to the CRF model to be used for building lattices
 *
 *
 */
CRF_ViterbiDecoder::CRF_ViterbiDecoder(CRF_FeatureStream* ftr_strm_in, CRF_Model* crf_in)
	: crf(crf_in),
	  ftr_strm(ftr_strm_in)
{
	this->nodeList= new CRF_StateVector();
	this->bunch_size=1;
	this->num_ftrs=this->ftr_strm->num_ftrs();
	this->num_labs=this->crf->getNLabs();
	this->ftr_buf = new float[num_ftrs*bunch_size];
	this->lab_buf = new QNUInt32[bunch_size];
	this->alpha_base = new double[this->num_labs];
	for (QNUInt32 i=0; i<this->num_labs; i++) {
		this->alpha_base[i]=0.0;
	}
	this->curViterbiWts = new vector<float>();
	this->prevViterbiWts = new vector<float>();
	this->curViterbiStateIds_new = new vector<uint>();
	this->prevViterbiStateIds_new = new vector<uint>();
}

/*
 * CRF_ViterbiDecoder destructor
 *
 *
 */
CRF_ViterbiDecoder::~CRF_ViterbiDecoder()
{
	delete [] this->ftr_buf;
	delete [] this->lab_buf;
	delete [] this->alpha_base;
	delete this->nodeList;
	delete this->curViterbiWts;
	delete this->prevViterbiWts;
	delete this->curViterbiStateIds_new;
	delete this->prevViterbiStateIds_new;
}

/*
 * CRF_ViterbiDecoder::getNodeList
 *
 * Accessor function for nodeList created during decode, if further processing
 * is desired.
 *
 */
CRF_StateVector * CRF_ViterbiDecoder::getNodeList() {
	return this->nodeList;
}

/*
 * CRF_ViterbiDecoder::internalStateUpdate
 *
 * Input: nodeCnt - index into nodeList for current state to be updated
 *        phn_id - phone label used in OpenFst lattice
 *        prevIdx - previous state Id
 *        prevWts - pointer to previous state weights
 *        curWts - pointer to current state weights to be updated
 *        curPtrs - point to vector of current state backwards pointers to be updated
 *
 * Returns: minimum weight value computed for this state update
 *
 * Performs an intra-state update of weights and backward pointers for the
 * Viterbi best-path decode.
 *
 * For a given phone label (phn_id), and a given current state (nodeCnt), updates
 * the possible paths into the state that result in that label.  This update
 * assumes a multi-state model in the underlying phone structure.
 *
 */
float CRF_ViterbiDecoder::internalStateUpdate(int nodeCnt, uint phn_id, int prevIdx,
												vector<float>* prevWts,
												vector<float>* curWts,
												vector<int>* curPtrs) {

	// Added by Ryan, just for debugging
//	cout << "Internal state update" << endl;

	QNUInt32 nStates = this->crf->getFeatureMap()->getNumStates();
	// Compute viterbi values for each of the states in our list of active prior states (prevStates)
	// Get the phone by looking at the ilabel of the arc
	uint phn_lab=phn_id-1;
	// should never be less than 0 - if there is this is a problem
	assert(phn_lab>=0);
	float min_wt=0.0;
	for (uint st=0; st<nStates; st++) {
		int state_idx=prevIdx*nStates+st;
		int cur_lab=phn_lab*nStates+st;
		// Next we go through the previous state ids and weights and propagate
		// them forward, adding in the appropriate weight
		if (cur_lab%nStates==0) {
			// if we're in a start state the only way we can get here is if
			// we came from the previous start state.  So we don't need to
			// make a decision - just propagate the weight forward
			float old_w = prevViterbiWts->at(state_idx);
			float trans_w;
			if (nodeCnt!=0) {
				trans_w=-1*this->nodeList->at(nodeCnt)->getFullTransValue(cur_lab,cur_lab);
			}
			else {
//				cout << "Getting state value for: " << cur_lab << endl;
				trans_w=-1*this->nodeList->at(nodeCnt)->getStateValue(cur_lab);
			}
			curWts->push_back(old_w+trans_w);
			curPtrs->push_back(state_idx);
			min_wt=old_w+trans_w;
		}
		else {
			// we're not in a start state, so we could get here one of two ways
			// check them both and take the min (negative log weights)
			float wt1,wt2;
			if (nodeCnt!=0) {
				wt1=prevViterbiWts->at(state_idx)+
						-1*this->nodeList->at(nodeCnt)->getFullTransValue(cur_lab,cur_lab);
				wt2=prevViterbiWts->at(state_idx-1)+
						-1*this->nodeList->at(nodeCnt)->getFullTransValue(cur_lab-1,cur_lab);
			}
			else {
				wt1=99999.0;
				wt2=99999.0;
			}
			if (wt1<wt2) {
				curWts->push_back(wt1);
				curPtrs->push_back(state_idx);
				if (wt1<min_wt) { min_wt=wt1; }
			}
			else {
				curWts->push_back(wt2);
				curPtrs->push_back(state_idx-1);
				if (wt2<min_wt) { min_wt=wt2; }
			}
		}

	}

	return min_wt;
}

/*
 * CRF_ViterbiDecoder::crossStateUpdate_new
 *
 * Input: check_state - index into nodeList for current state to be updated
 *        end_idx - phone label used in OpenFst lattice
 *        transw - previous state Id
 *        phn_lab - pointer to previous state weights
 *        wrd_lab - pointer to current state weights to be updated
 *
 *
 * Performs an inter-state update of weights and backward pointers for the
 * Viterbi best-path decode.
 *
 * For a given phone label (phn_id), and a given previous state (end_idx), updates
 * the possible paths into the state that result in that label from the previous
 * state.
 *
 */
void CRF_ViterbiDecoder::crossStateUpdate_new(uint check_state, uint end_idx,
												float transw, uint phn_lab, uint wrd_lab)
{
	QNUInt32 nStates = this->crf->getFeatureMap()->getNumStates();

	// Changed by Ryan, for tmpViterbiStateIdMap_new
//	map<uint,uint>::iterator tmpVitStateIt;
	map<CRF_ComposedLatState,uint>::iterator tmpVitStateIt;

	int counter=0;

	// if we're not over the min_weight+beam width, we're going to be
	// pruned down stream anyway.  Not doing this here saves us
	// some effort later.

	// Changed by Ryan, for tmpViterbiStateIdMap_new
//	tmpVitStateIt=tmpViterbiStateIdMap_new.find(check_state);
	CRF_ComposedLatState composed_lat_state(check_state,phn_lab);
	tmpVitStateIt=tmpViterbiStateIdMap_new.find(composed_lat_state);

	if (tmpVitStateIt == tmpViterbiStateIdMap_new.end()) {
		addedCounter++;

		// the above means that it is not in our expansion list
		// so we expand it as new and add it
		tmpViterbiStateIds_new.push_back(check_state);
		tmpViterbiPhnIds.push_back(phn_lab);
		tmpViterbiWrdIds.push_back(wrd_lab);

		// Added by Ryan
		tmpIsPhoneStartBoundary.push_back(true);

		// Changed by Ryan, for tmpViterbiStateIdMap_new
//		tmpViterbiStateIdMap_new[check_state]=tmpViterbiStateIds_new.size()-1;
		tmpViterbiStateIdMap_new[composed_lat_state]=tmpViterbiStateIds_new.size()-1;

		tmpPruneWts.push_back(transw);
		for (uint st=0; st<nStates; st++) {
			if (st==0) 	{
				tmpViterbiWts.push_back(transw);
				tmpViterbiPtrs.push_back(end_idx);
			}
			else {
				// These need to be here to hold our position
				tmpViterbiWts.push_back(99999.0);
				tmpViterbiPtrs.push_back(-1);
			}
		}
	}
	else {
		updateCheckCounter++;

		uint exp_idx=tmpVitStateIt->second;
		int exp_wt_idx=exp_idx*nStates;

		// Changed by Ryan
		if (transw<tmpViterbiWts[exp_wt_idx]) {
//		if (transw<tmpPruneWts[exp_idx]) {

			updateCounter++;

			// this transition is better than the one we've previously
			// expanded on, so replace the one we've got with it

			// Added by Ryan
			if (tmpViterbiPhnIds[exp_idx] == phn_lab)
			{
				crossUpdateSamePhoneCounter++;
			}
			else
			{
				crossUpdateDiffPhoneCounter++;
			}
			if (tmpViterbiWrdIds[exp_idx] == wrd_lab)
			{
				crossUpdateSameWordCounter++;
			}
			else
			{
				crossUpdateDiffWordCounter++;
			}
			if (tmpViterbiStateIds_new[exp_idx] == check_state)
			{
				crossUpdateSameStateCounter++;
			}
			else
			{
				crossUpdateDiffStateCounter++;
			}

			// Changed by Ryan
			tmpViterbiWts[exp_wt_idx]=transw;
			tmpViterbiPtrs[exp_wt_idx]=end_idx;
//			for (uint st=0; st<nStates; st++) {
//				if (st==0) 	{
//					tmpViterbiWts[exp_wt_idx + st]=transw;
//					tmpViterbiPtrs[exp_wt_idx + st]=end_idx;
//				}
//				else {
//					// These need to be here to hold our position
//					tmpViterbiWts[exp_wt_idx + st]=99999.0;
//					tmpViterbiPtrs[exp_wt_idx + st]=-1;
//				}
//			}

			// Added by Ryan
			tmpIsPhoneStartBoundary[exp_idx] = true;

			// Changed by Ryan
			tmpPruneWts[exp_idx]=transw;
//			if (transw<tmpPruneWts[exp_idx]) {
//				tmpPruneWts[exp_idx]=transw;
//			}

			tmpViterbiPhnIds[exp_idx]=phn_lab;
			tmpViterbiWrdIds[exp_idx]=wrd_lab;
		}
	}
}

// added by Ryan
void CRF_ViterbiDecoder::createFreePhoneLmFst(VectorFst<StdArc>* new_lm_fst)
{
	if (new_lm_fst == NULL)
	{
		string errstr="CRF_ViterbiDecoder::createFreePhoneLmFst() caught exception: The lm fst passed in is null. It needs to be initialized first.";
		throw runtime_error(errstr);
	}
	StateId start_state = new_lm_fst->AddState();   // 1st state will be state 0 (returned by AddState)
	new_lm_fst->SetStart(start_state);  // arg is state ID
//	StateId final_state = new_lm_fst->AddState();
//	new_lm_fst->SetFinal(final_state, 0);

	QNUInt32 nStates = this->crf->getFeatureMap()->getNumStates();
	int nPhoneClasses = this->num_labs / nStates;
	if (this->num_labs % nStates != 0)
	{
		string errstr="CRF_ViterbiDecoder_StdSeg_NoSegTransFtr::createFreePhoneLmFst() caught exception: nActualLabs % nStates != 0.";
		throw runtime_error(errstr);
	}
//	for (uint cur_lab = 0; cur_lab < nPhoneClasses; cur_lab++)
//	{
//		StateId prev_state = start_state;
//		for (uint st = 0; st < nStates; st++)
//		{
//			uint cur_lab_nState = cur_lab * nStates + st;
//			StateId cur_state = new_lm_fst->AddState();
//
//			//transition from previous phone internal state to current phone internal state
//			new_lm_fst->AddArc(prev_state, StdArc(cur_lab_nState+1,0,0,cur_state));
//
//			//self transition for current phone internal state
//			new_lm_fst->AddArc(cur_state, StdArc(cur_lab_nState+1,0,0,cur_state));
//
//			prev_state = cur_state;
//		}
//
//		// transition from the final phone internal state to the lm fst start state
//		new_lm_fst->AddArc(prev_state, StdArc(0,0,0,start_state));
//
//		// transition from the final phone internal state to the lm fst final state,
//		// transducing the word level symbol "!SENT_END" (corresponding the state index "1"),
//		// indicating the end of the sentence
//		new_lm_fst->AddArc(prev_state, StdArc(0,1,0,final_state));
//	}
	for (uint cur_lab = 0; cur_lab < nPhoneClasses; cur_lab++)
	{
		StateId cur_state = new_lm_fst->AddState();

		// transition from start state to current phone state
		new_lm_fst->AddArc(start_state, StdArc(cur_lab+1,cur_lab+1,0,cur_state));

		// if nStates == 1, not allowed to have no two same phones in a row, so each phone state does not have self transition
		// for nStates > 1, each phone state can transit to any phone state, so just add an arc to start state.
		if (nStates > 1)
		{
			// transition from current phone state to start state
			new_lm_fst->AddArc(cur_state, StdArc(0,0,0,start_state));
		}

		// every single phone could be a final state
		new_lm_fst->SetFinal(cur_state, 0);
	}
	// if nStates == 1, not allowed to have no two same phones in a row, so each phone state does not have self transition
	if (nStates == 1)
	{
		for (uint cur_lab = 0; cur_lab < nPhoneClasses; cur_lab++)
		{
			StateId cur_state = start_state + cur_lab + 1;
			for (uint next_lab = 0; next_lab < nPhoneClasses; next_lab++)
			{
				if (cur_lab != next_lab)
				{
					StateId next_state = start_state + next_lab + 1;
					// transition from current phone state to next phone state
					new_lm_fst->AddArc(cur_state, StdArc(next_lab+1,next_lab+1,0,next_state));
				}
			}
		}
	}
}

/*
 * CRF_ViterbiDecoder::nStateDecode
 *
 * Input: *result_fst - empty fst to store final result
 *        *lm_fst - fst containing the language model to decode against
 *        input_beam - beam width to decode against
 *
 *  Performs Viterbi decoding using time-synchronous pruning controlled by the
 *  input_beam parameter and constrained by the language model in lm_fst.
 *  result_fst contains a finite-state transducer with the
 *  single best word sequence as determined by the Viterbi decode.
 *
 *  NOTE:  Currently this function is hard-coded to assume that the end of
 *  sentence token (e.g. SENT_END) is mapped to symbol 1 in the language model.
 *  This should be updated to take a parameter that holds the value of the end
 *  of sentence token.
 *
 *
 */
int CRF_ViterbiDecoder::nStateDecode(VectorFst<StdArc>* result_fst, VectorFst<StdArc>* lm_fst, double input_beam, uint min_hyps, uint max_hyps, float beam_inc) {

	// Takes in an empty fst and an fst containing a dictionary/language model network
	// both of these should be on the tropical semiring
	// Returns a weighted fst containing the best path through the current utterance

	QNUInt32 nStates = this->crf->getFeatureMap()->getNumStates();
	QNUInt32 ftr_count;
	VectorFst<StdArc>* fst = new VectorFst<StdArc>();
	StateId startState=fst->AddState();   // 1st state will be state 0 (returned by AddState)
	fst->SetStart(startState);  // arg is state ID
	bool prune=true;
	if (input_beam<=0.0) { prune=false;}

	// added by Ryan
	// if input lm fst is null, we need to build a free phone lm fst
	bool input_lm_fst_is_null = false;
	if (lm_fst == NULL)
	{
		input_lm_fst_is_null = true;
		lm_fst = new VectorFst<StdArc>();
		createFreePhoneLmFst(lm_fst);
	}

	// first we need to set up our hypothesis list and backtracking pointer
	// The "word" id deque gives us the list of states that we are considering at this
	// timestep and is used to index the word_starts and word_wts deques.
	// The deques in each of these vectors
	vector<uint> viterbiStartPhns;
	vector<uint> viterbiStartWrds;
	curViterbiWts->clear();
	prevViterbiWts->clear();
	curViterbiStateIds_new->clear();
	prevViterbiStateIds_new->clear();

	// we look for arcs off the start state and make them hypotheses
	StateIterator< VectorFst<StdArc> > lmIter(*lm_fst);
	StateId lm_start=lmIter.Value();
	deque<uint> expansionList;
	deque<float> expansionWt;

	expansionList.push_back(lm_start);
	expansionWt.push_back(0.0);

	while (!expansionList.empty()) {
		uint exp_state=expansionList.front();expansionList.pop_front();
		float exp_wt=expansionWt.front(); expansionWt.pop_front();
	for (ArcIterator< VectorFst<StdArc> > aiter(*lm_fst,exp_state); !aiter.Done(); aiter.Next()) {

		if (aiter.Value().ilabel==0) {
			expansionList.push_back(aiter.Value().nextstate);
			expansionWt.push_back(aiter.Value().weight.Value()+exp_wt);
		}
		else {

		prevViterbiStateIds_new->push_back(aiter.Value().nextstate);
		viterbiStartPhns.push_back(aiter.Value().ilabel);
		viterbiStartWrds.push_back(aiter.Value().olabel);

		for (uint state=0; state<nStates; state++) {
			if (state==0) {
				// changed by Ryan, the accumulated weight should be included.
				//prevViterbiWts->push_back(aiter.Value().weight.Value());
				prevViterbiWts->push_back(aiter.Value().weight.Value() + exp_wt);
			}
			else {
				prevViterbiWts->push_back(99999.0);
			}
		}
		}

	}
	}


	int nodeCnt=0;
	int num_pruned=0;
	double min_weight=99999.0; // infinity for our initial minimum weight
	double prev_min_weight=min_weight;
	int totalStates=0;
	double beam=input_beam;
	uint int_time=0;
	uint cross_time=0;
	uint merge_time=0;

	// Added by Ryan
	updateCounter=0;
	epsilonCounter=0;
	addedCounter=0;
	checkedCounter=0;
	updateCheckCounter=0;
	dupCounter=0;

	// Added by Ryan
	crossUpdateSamePhoneCounter = 0;
	crossUpdateDiffPhoneCounter = 0;
	crossUpdateSameWordCounter = 0;
	crossUpdateDiffWordCounter = 0;
	crossUpdateSameStateCounter = 0;
	crossUpdateDiffStateCounter = 0;

	time_t loopstart = time(NULL);
	do {
		ftr_count=ftr_strm->read(this->bunch_size,ftr_buf,lab_buf);

		for (QNUInt32 i=0; i<ftr_count; i++) {
			float* new_buf = new float[this->num_ftrs];
			for (QNUInt32 j=0; j<this->num_ftrs; j++) {
				int idx=i*this->num_ftrs+j;
				new_buf[j]=ftr_buf[idx];
			}
			this->nodeList->set(nodeCnt,new_buf,num_ftrs,this->lab_buf[i],this->crf);
			float value=this->nodeList->at(nodeCnt)->computeTransMatrix();
			double scale;
			double* prev_alpha;
			if (nodeCnt == 0) {
				prev_alpha=this->alpha_base;
				scale=this->nodeList->at(nodeCnt)->computeFirstAlpha(prev_alpha);
			}
			else {
				prev_alpha=this->nodeList->at(nodeCnt-1)->getAlpha();
				scale=this->nodeList->at(nodeCnt)->computeAlpha(prev_alpha);
			}

			// Now we do our forward viterbi processing
			// First make sure the various bookkeeping vectors are empty before we begin.
			//cout << "Clearing viterbi bookkeeping structs" << endl;
			//this->nodeList->at(nodeCnt)->viterbiStates.clear();
			this->nodeList->at(nodeCnt)->viterbiPointers.clear();
			curViterbiWts->clear();
			curViterbiStateIds_new->clear();
			tmpViterbiStateIds_new.clear();
			tmpViterbiStateIdMap_new.clear();
			tmpViterbiPhnIds.clear();
			tmpViterbiWrdIds.clear();
			tmpViterbiWts.clear();
			tmpViterbiPtrs.clear();
			tmpPruneWts.clear();
			tmpViterbiStateIds_new.reserve(prevViterbiWts->size());
			tmpViterbiPhnIds.reserve(prevViterbiWts->size());
			tmpViterbiWts.reserve(prevViterbiWts->size());
			tmpViterbiPtrs.reserve(prevViterbiWts->size());
			tmpPruneWts.reserve(prevViterbiWts->size());

			// Added by Ryan
			tmpIsPhoneStartBoundary.clear();
			tmpIsPhoneStartBoundary.reserve(prevViterbiStateIds_new->size());

			uint epsAdded=0;
			uint epsModified=0;
			uint epsMapSize=0;
			min_weight=99999.0;
			if (nodeCnt==0) {
				// If we're in the first timestep we need to send our initial values and not our prior
				// node values.
				// We also only need to do the state internal update because we know that we can't
				// be transitioning to a new phone here.
				// We store these in the temporary vectors to make pruning easier below
				for (uint idx=0; idx<prevViterbiStateIds_new->size(); idx++) {

					uint tmp_state=prevViterbiStateIds_new->at(idx);
					curViterbiStateIds_new->push_back(tmp_state);
					this->nodeList->at(nodeCnt)->viterbiPhnIds.push_back(viterbiStartPhns[idx]);
					prevViterbiWrdIds.push_back(viterbiStartWrds[idx]);
					double min_wt=internalStateUpdate(nodeCnt, viterbiStartPhns[idx], idx,
							prevViterbiWts, curViterbiWts, &(this->nodeList->at(nodeCnt)->viterbiPointers));
					if (min_wt<min_weight) { min_weight=min_wt; }

					// Added by Ryan
					this->nodeList->at(nodeCnt)->isPhoneStartBoundary.push_back(true);
				}
			}
			else {
				time_t time1 = time(NULL);
				//cout << "Internal update" << endl;
				for (uint idx=0; idx<prevViterbiStateIds_new->size(); idx++) {

					uint tmp_state=prevViterbiStateIds_new->at(idx);
					uint tmp_phn=this->nodeList->at(nodeCnt-1)->viterbiPhnIds[idx];
					tmpViterbiStateIds_new.push_back(tmp_state);
					tmpViterbiPhnIds.push_back(tmp_phn);
					tmpViterbiWrdIds.push_back(prevViterbiWrdIds[idx]);

					// Added by Ryan
					tmpIsPhoneStartBoundary.push_back(false);

					// Changed by Ryan, for tmpViterbiStateIdMap_new
//					tmpViterbiStateIdMap_new[tmp_state]=tmpViterbiStateIds_new.size()-1;
					CRF_ComposedLatState composed_lat_state(tmp_state,tmp_phn);
					tmpViterbiStateIdMap_new[composed_lat_state]=tmpViterbiStateIds_new.size()-1;

					float min_wt=internalStateUpdate(nodeCnt, tmp_phn, idx,
							prevViterbiWts, &(tmpViterbiWts),&(tmpViterbiPtrs));
					tmpPruneWts.push_back(min_wt);
					if (min_wt<min_weight) { min_weight=min_wt; }
				}
				time_t time2=time(NULL);
				int_time+=time2-time1;
				updateCounter=0;
				epsilonCounter=0;
				addedCounter=0;
				checkedCounter=0;
				updateCheckCounter=0;
				dupCounter=0;
				epsMapSize=0;

				// Added by Ryan
				crossUpdateSamePhoneCounter = 0;
				crossUpdateDiffPhoneCounter = 0;
				crossUpdateSameWordCounter = 0;
				crossUpdateDiffWordCounter = 0;
				crossUpdateSameStateCounter = 0;
				crossUpdateDiffStateCounter = 0;

				deque<CRF_ViterbiState> epsList;
				map<CRF_ViterbiState,uint> epsMap;
				map<CRF_ViterbiState,uint>::iterator epsIt;

				epsAdded=0;
				epsModified=0;

				//cout << "Cross state update" << endl;
				time1=time(NULL);
				for (uint idx=0; idx<prevViterbiStateIds_new->size(); idx++) {
					uint prev_state=prevViterbiStateIds_new->at(idx);
					uint prev_phn=this->nodeList->at(nodeCnt-1)->viterbiPhnIds[idx];
					int end_idx=idx*nStates+nStates-1;
					if (prevViterbiWts->at(end_idx)<prev_min_weight+beam || !prune) {
						//only perform this update if our exit state from the previous iteration
						// is under the pruning threshold - cut down on unneeded expansions that
						// will only be pruned anyway.
						//cout << "Expanding" << endl;
						deque<uint> expansionList;
						deque<float> expansionWt;
						uint prev_lab=(prev_phn-1)*nStates+nStates-1;
						int end_idx=idx*nStates+nStates-1;

						expansionList.push_back(prev_state);
						expansionWt.push_back(prevViterbiWts->at(end_idx));
						int counter=0;
						uint exp_state=prev_state;
						float exp_wt=prevViterbiWts->at(end_idx);

						//cout << "Starting LM iteration" << endl;
						for (ArcIterator< VectorFst<StdArc> > aiter(*lm_fst,exp_state);
										!aiter.Done();
										aiter.Next()) {
							counter++;
							uint nextstate=aiter.Value().nextstate;
							uint check_state=nextstate;
							if (aiter.Value().ilabel == 0) {
								//cout << "Epsilon handling" << endl;
								// epsilon transition - put the arc at the end of the expansionList and
								// expand it later
								float new_wt = exp_wt+aiter.Value().weight.Value();
								// first check the epsMap and see if we already have a representative
								// for this state
								CRF_ViterbiState test_state(check_state,prev_lab,new_wt,end_idx);
								epsIt=epsMap.find(test_state);
								if (epsIt == epsMap.end()) {
									epsList.push_back(test_state);
									epsMap[test_state]=epsList.size()-1;
									epsAdded++;
								}
								else {
									uint eps_idx=epsIt->second;
									if (new_wt < epsList[eps_idx].weight) {
										epsList[eps_idx].weight=new_wt;
										epsList[eps_idx].end_idx=end_idx;
									}
									epsModified++;
								}
								//cout << "Epsilon handling ended" << endl;
							}
							else {
								//cout << "Expansion" << endl;
								checkedCounter++;
								// non-epsilon transition
								// First check to see if we've already done this arc on this cycle
								// First figure out our values
								int cur_lab=(aiter.Value().ilabel-1)*nStates;
								float transw=-1*this->nodeList->at(nodeCnt)->getFullTransValue(prev_lab,cur_lab);
								// add in the value on this arc
								// and all arcs travelled to get to this point
								transw+=aiter.Value().weight.Value()+exp_wt;
								if (transw<min_weight) {min_weight=transw;}
								// see if we need to add it to our list of
								// currently active transitions or if we just need to check a previously
								// added transition
								if (transw<min_weight+beam || !prune) {
									crossStateUpdate_new(check_state, end_idx, transw, aiter.Value().ilabel, aiter.Value().olabel);
								}
								//cout << "Expansion ended" << endl;
							}
						}
						//cout << "LM ended" << endl;
					}
					//cout << "Weight check ended" << endl;
				}
				//cout << "Cross state update ends" << endl;

				//cout << "Epsilon update" << endl;
				uint eps_offset=0;
				while (!epsList.empty()) {
					CRF_ViterbiState eps_state = epsList.front(); epsList.pop_front();
					eps_offset++;
					epsMap.erase(eps_state);
					uint exp_state=eps_state.state;
					uint prev_lab=eps_state.label;
					float exp_wt=eps_state.weight;
					uint end_idx=eps_state.end_idx;
					for (ArcIterator< VectorFst<StdArc> > aiter(*lm_fst,exp_state);
																!aiter.Done();
																aiter.Next()) {

						uint nextstate=aiter.Value().nextstate;
						uint check_state=nextstate;
						if (aiter.Value().ilabel == 0) {
							// epsilon transition - put the arc at the end of the expansionList and
							// expand it later
							float new_wt = exp_wt+aiter.Value().weight.Value();
							// first check the epsMap and see if we already have a representative
							// for this state
							CRF_ViterbiState test_state(check_state,prev_lab,new_wt,end_idx);
							//epsList.push_back(test_state);

							epsIt=epsMap.find(test_state);
							if (epsIt == epsMap.end()) {
								epsList.push_back(test_state);
								epsMap[test_state]=epsList.size()+eps_offset-1;
								//adding eps_offset ensures that our list remains in synch
								epsAdded++;
							}
							else {
								uint eps_idx=epsIt->second-eps_offset;
								// we have to correct for the fact that we've removed eps_offset
								// elements from our list
								if (new_wt < epsList[eps_idx].weight) {
									epsList[eps_idx].weight=new_wt;
									epsList[eps_idx].end_idx=end_idx;
								}
								epsModified++;
							}

							//expansionList.push_back(check_state);
							//expansionWt.push_back(new_wt);
							epsilonCounter++;
						}
						else {
							checkedCounter++;
							// non-epsilon transition
							// First check to see if we've already done this arc on this cycle
							// First figure out our values
							int cur_lab=(aiter.Value().ilabel-1)*nStates;
							float transw=-1*this->nodeList->at(nodeCnt)->getFullTransValue(prev_lab,cur_lab);
							// add in the value on this arc
							// and all arcs travelled to get to this point
							transw+=aiter.Value().weight.Value()+exp_wt;
							if (transw<min_weight) {min_weight=transw;}
							// see if we need to add it to our list of
							// currently active transitions or if we just need to check a previously
							// added transition
							if (transw<min_weight+beam || !prune) {
								crossStateUpdate_new(check_state, end_idx, transw, aiter.Value().ilabel, aiter.Value().olabel);
							}
						}
					}
				}
				//cout << "Epsilon update ends" << endl;

				time2=time(NULL);
				cross_time+=time2-time1;
				epsMapSize=epsMap.size();

				num_pruned=0;
				// Now that we have our minimum weights at this time frame, we need to go through
				// our list again to see what we will keep and what we will prune
				//this->nodeList->at(nodeCnt)->viterbiStateIds.reserve(tmpViterbiStateIds.size());
				this->nodeList->at(nodeCnt)->viterbiPhnIds.reserve(tmpViterbiPhnIds.size());
				//this->nodeList->at(nodeCnt)->viterbiPhnIds.reserve(tmpViterbiStateIds.size());
				this->nodeList->at(nodeCnt)->viterbiPointers.reserve(tmpViterbiPtrs.size());

				// Added by Ryan
				this->nodeList->at(nodeCnt)->isPhoneStartBoundary.reserve(tmpIsPhoneStartBoundary.size());

				curViterbiWts->reserve(tmpViterbiWts.size());
				curViterbiStateIds_new->reserve(prevViterbiStateIds_new->size());
				//curViterbiStateIds->reserve(prevViterbiStateIds->size());
				prevViterbiWrdIds.clear();

				//cout << "final move" << endl;
				time1=time(NULL);
				for (uint idx=0; idx<tmpViterbiStateIds_new.size(); idx++) {
				//for (uint idx=0; idx<tmpViterbiStateIds.size(); idx++) {
					// First, check to see if we want to keep this state at all
					int wt_idx=idx*nStates;
					uint tmp_state = tmpViterbiStateIds_new[idx];
					//CRF_ViterbiState tmp_state=tmpViterbiStateIds[idx];
					uint tmp_phn = tmpViterbiPhnIds[idx];
					//uint tmp_phn = tmp_state.label;
					//float tmp_wt = tmp_state.weight;
					if (tmpPruneWts[idx]<min_weight+beam || !prune) {
						// We'll keep this for the next iteration
						curViterbiStateIds_new->push_back(tmp_state);
						prevViterbiWrdIds.push_back(tmpViterbiWrdIds[idx]);
						this->nodeList->at(nodeCnt)->viterbiPhnIds.push_back(tmp_phn);
						for (uint st=0; st<nStates; st++) {
							curViterbiWts->push_back(tmpViterbiWts[wt_idx+st]);
							this->nodeList->at(nodeCnt)->viterbiPointers.push_back(tmpViterbiPtrs[wt_idx+st]);
						}

						// Added by Ryan
						bool isStartBound = tmpIsPhoneStartBoundary[idx];
						this->nodeList->at(nodeCnt)->isPhoneStartBoundary.push_back(isStartBound);
					}
					else {
						num_pruned++;
					}
				}
				time2=time(NULL);
				merge_time+=time2-time1;
				//cout << "Final move ends" << endl;
			}
			tmpViterbiWts.clear();
			tmpViterbiPtrs.clear();
			tmpViterbiStateIds_new.clear();
			tmpViterbiPhnIds.clear();
			tmpViterbiWrdIds.clear();
			tmpPruneWts.clear();

			// Added by Ryan
			tmpIsPhoneStartBoundary.clear();

			prev_min_weight=min_weight;
			totalStates+=curViterbiStateIds_new->size();

			// As a check on where we are, let's dump the current ViterbiStates and Weights at every
			// cycle.  Just to see if what we're doing looks sane
			double average=(totalStates/(nodeCnt+1.0));

			// changed by Ryan
			//if (nodeCnt%50 == 0) {
			if (nodeCnt%100 == 0) {
//			if (nodeCnt%1 == 0) {
			    cout << "time: " << nodeCnt << " " << curViterbiStateIds_new->size() << " hyps ";
				//cout << "time: " << nodeCnt << " " << curViterbiStateIds->size() << " hyps ";
			    cout << curViterbiWts->size() << " states " << prevViterbiWts->size() << " prev_states";
			    cout << " pruned away " << num_pruned << " this cycle" << endl;
			    cout << "       checked: " << checkedCounter << " added: " << addedCounter;
			    cout << " updated: " << updateCounter << endl;
			    cout << "       checkUpdate: " << updateCheckCounter;
			    cout << " epsilons: " << epsilonCounter << endl;
			    cout << "      Average hyps per timestep: " << average;
			    cout << "      Current pruning beam: " << beam << endl;
			    cout << " epsAdded: " << epsAdded << "  epsModified: " << epsModified << endl;
			    cout << " epsMapSize: " << epsMapSize << endl;
				cout << "Internal update time: " << int_time;
				cout << " Cross update time: " << cross_time;
				cout << " Merge time: " << merge_time << endl;

				// Added by Ryan
				cout << "Cross update: " << crossUpdateSamePhoneCounter << " same phones, "
						<< crossUpdateDiffPhoneCounter << " different phones, "
						<< crossUpdateSameWordCounter << " same words, "
						<< crossUpdateDiffWordCounter << " different words, "
						<< crossUpdateSameStateCounter << " same states, "
						<< crossUpdateDiffStateCounter << " different states."
						<< endl;
			}



			// Now swap the current and previous vectors.
			vector<float>* holdWts=curViterbiWts;
			curViterbiWts=prevViterbiWts;
			prevViterbiWts=holdWts;

			vector<uint>* holdStates=curViterbiStateIds_new;
			curViterbiStateIds_new=prevViterbiStateIds_new;
			prevViterbiStateIds_new=holdStates;
			nodeCnt++;
		}
	} while (ftr_count >= this->bunch_size);
	time_t loopend=time(NULL);
	cout << "Internal update time: " << int_time;
	cout << " Cross update time: " << cross_time;
	cout << " Merge time: " << merge_time << endl;
	cout << " Total loop time: " << (loopend-loopstart) << endl;
	this->nodeList->setNodeCount(nodeCnt);
	double Zx=this->nodeList->at(nodeCnt-1)->computeAlphaSum();
	int final_state = fst->AddState();
	fst->SetFinal(final_state,Zx);

	// Backtrack through the decoder to build the output lattice
	// First search through the final timestep and find the best result that ends in a final state
	min_weight=99999;
	int min_idx=-1;
	cout << "checking through " << prevViterbiStateIds_new->size() << " states for final state " << endl;
	//cout << "checking through " << prevViterbiStateIds->size() << " states for final state " << endl;
	int fstate_cnt=0;
	// the if condition is added by Ryan, originally unconditional.
	if (!input_lm_fst_is_null)
	{

		for (uint idx=0; idx<prevViterbiStateIds_new->size(); idx++) {
		//for (uint idx=0; idx<prevViterbiStateIds->size(); idx++) {
			uint tmp_state=prevViterbiStateIds_new->at(idx);
			//CRF_ViterbiState tmp_state=prevViterbiStateIds->at(idx);
			uint tmp_wrd=prevViterbiWrdIds[idx];
			//uint tmp_wrd=tmp_state.wrd_label;
				if (tmp_wrd==1) {
					fstate_cnt++;
					int state_idx = idx*nStates+nStates-1;
					float weight=prevViterbiWts->at(state_idx);
					if (weight<min_weight) {
						min_weight=weight;
						min_idx=state_idx;
					}
				}
		}

	}
	// the whole else part is added by Ryan
	else
	{
		for (uint idx=0; idx<prevViterbiStateIds_new->size(); idx++) {
			uint tmp_state=prevViterbiStateIds_new->at(idx);
//			if (lm_fst->isFinalState(tmp_state)) // TODO: if tmp_state_id is a final state
//			{
				fstate_cnt++;
				int state_idx = idx*nStates+nStates-1;
				float weight=prevViterbiWts->at(state_idx);
				if (weight<min_weight) {
					min_weight=weight;
					min_idx=state_idx;
				}
//			}
		}
	}

	// changed by Ryan
	//cout << "Found " << fstate_cnt << " final states " << min_idx << endl;
	cout << "Found " << fstate_cnt << " final states " << min_idx << " with weight " << min_weight << endl;

	curViterbiWts->clear();
	prevViterbiWts->clear();
	curViterbiStateIds_new->clear();
	prevViterbiStateIds_new->clear();


	int cur_state=fst->AddState();
	int next_state=final_state;
	if (min_idx<0) {
		// Failsafe - create a null transition
		cout << "ERROR: Could not reach end of utterance" << endl;
		fst->AddArc(startState,StdArc(0,0,8,final_state));
	}
	else {

		//cout << "Reverse label sequence: ";

	for (int idx=nodeCnt-1; idx>=0; idx--) {
		int cur_min_idx_ptr=min_idx/nStates;
		int cur_min_idx_offset=min_idx%nStates;
		uint cur_lab=this->nodeList->at(idx)->viterbiPhnIds[cur_min_idx_ptr]-1;
		uint cur_lab_ptr=cur_lab*nStates+cur_min_idx_offset;

		if (idx>0) {
			int prev_min_idx=this->nodeList->at(idx)->viterbiPointers[min_idx];
			int prev_min_idx_ptr=prev_min_idx/nStates;
			int prev_min_idx_offset=prev_min_idx%nStates;
			uint prev_lab=this->nodeList->at(idx-1)->viterbiPhnIds[prev_min_idx_ptr]-1;
			uint prev_lab_ptr=prev_lab*nStates+prev_min_idx_offset;
			float trans_w=-1*this->nodeList->at(idx)->getFullTransValue(prev_lab_ptr,cur_lab_ptr);

			// Changed by Ryan
			//
			// The original way does not apply to the case when nStates == 1
			// So we have to use the new way to include that case as well.
			//
			// ********* This is the old way. *********
//			// Now we add the arc.  We need to be careful about how we label it.  If we're making
//			// a transition from an end state to a start state, then we need to put the label for
//			// the new phone on this arc
//			if (cur_min_idx_offset==0 && prev_min_idx_offset==nStates-1) {
			// ****************************************
			//
			// ********* This is the new way. *********
			// Now we add the arc. We put the label for the new phone on this arc only when the
			// hypothesized phone state of the current node is the start boundary of the current
			// phone, which also means the hypothesized phone state of the previous node is the
			// end boundary of the previous phone.
			if (cur_min_idx_offset == 0 && this->nodeList->at(idx)->isPhoneStartBoundary[cur_min_idx_ptr]) {
			// ****************************************
				//cout << idx << " cur_lab: " << cur_lab << endl;
				fst->AddArc(cur_state,StdArc(cur_lab_ptr+1,cur_lab+1,trans_w,next_state));

				//cout << " " << cur_lab;
			}
			else {
				//cout << idx << " cur_lab: " << cur_lab << endl;
				fst->AddArc(cur_state,StdArc(cur_lab_ptr+1,0,trans_w,next_state));
			}
			min_idx=prev_min_idx;
			next_state=cur_state;
			cur_state=fst->AddState();
		}
		else {
			// we're in our first timestep, so we add an arc that just has the state value here
			//cout << idx << " cur_lab: " << cur_lab << endl;
			float trans_w=-1*this->nodeList->at(idx)->getStateValue(cur_lab_ptr);
			fst->AddArc(startState,StdArc(cur_lab_ptr+1,cur_lab+1,trans_w,next_state));

			//cout << " " << cur_lab;
		}
		this->nodeList->at(idx)->viterbiPointers.clear();
		this->nodeList->at(idx)->viterbiPhnIds.clear();

		// Added by Ryan
		this->nodeList->at(idx)->isPhoneStartBoundary.clear();
	}

		//cout << endl;
	}
	Connect(fst);
	Compose(*fst,*lm_fst,result_fst);

	//cout << "result_fst has " << result_fst->NumStates() << " states." << endl;

	delete fst;

	// added by Ryan, delete the free phone lm fst if created
	if (input_lm_fst_is_null)
		delete lm_fst;

	return nodeCnt;
}

