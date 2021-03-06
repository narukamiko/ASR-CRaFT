/*
 * CRF_StdSegNStateNode_WithoutDurLab.cpp
 *
 *  Created on: May 2, 2012
 *      Author: Yanzhang (Ryan) He
 */

#include "CRF_StdSegNStateNode_WithoutDurLab.h"

CRF_StdSegNStateNode_WithoutDurLab::CRF_StdSegNStateNode_WithoutDurLab(float* fb, QNUInt32 sizeof_fb, QNUInt32 lab, CRF_Model* crf, QNUInt32 nodeMaxDur, QNUInt32 prevNode_nLabs, QNUInt32 nextNode_nActualLabs)
: CRF_StdSegNStateNode(fb, sizeof_fb, lab, crf, nodeMaxDur, prevNode_nLabs, nextNode_nActualLabs) {

	this->nActualLabs = nLabs;
	this->numAvailLabs = this->nActualLabs;

	this->alphaArray_WithDur = new double[this->nActualLabs * this->nodeLabMaxDur];
//	for (QNUInt32 id = 0; id < this->nActualLabs * this->nodeLabMaxDur; id++)
//	{
//		this->alphaArray_WithDur[id] = CRF_LogMath::LOG0;
//	}

	delete [] this->stateArray;
	this->stateArray = new double[this->nActualLabs * this->nodeLabMaxDur];
//	for (QNUInt32 id = 0; id < this->nActualLabs * this->nodeLabMaxDur; id++)
//	{
//      // Changed by Ryan. It should be CRF_LogMath::LOG0 instead of 0.0. TODO: verify.
//		//this->stateArray[id] = 0.0;
//      this->stateArray[id] = CRF_LogMath::LOG0;
//	}

	delete [] this->denseTransMatrix;
	delete [] this->diagTransMatrix;
	delete [] this->offDiagTransMatrix;
	this->denseTransMatrix = new double[nFullLabs * nFullLabs * this->nodeLabMaxDur]; // Dense transition matrix
	this->diagTransMatrix = new double[nLabs * this->nodeLabMaxDur];
	this->offDiagTransMatrix = new double[nLabs * this->nodeLabMaxDur];

	delete [] this->tempBeta;
	this->tempBeta = new double[this->nActualLabs * this->labMaxDur];  // this tempBeta is beta(phone, dur, endtime)*stateValue(phone, dur, endtime) for current node, which is different from tempBeta in CRF_StdSegStateNode.
//	for (QNUInt32 id = 0; id < this->nActualLabs * this->labMaxDur; id++)
//	{
//		this->tempBeta[id] = CRF_LogMath::LOG0;
//	}

	delete [] this->logAddAcc;
	// this->nActualLabs is for cross phone transitions,
	// 2 is for the self transition and the transition from previous internal state.
	this->logAddAcc = new double[(this->nActualLabs + 2) * this->labMaxDur];
//	for (QNUInt32 id = 0; id < (this->nActualLabs + 2) * this->labMaxDur; id++)
//	{
//		this->logAddAcc[id] = CRF_LogMath::LOG0;
//	}

}

CRF_StdSegNStateNode_WithoutDurLab::~CRF_StdSegNStateNode_WithoutDurLab() {

	delete [] this->alphaArray_WithDur;

	// all other arrays are deleted in the base classes.
}

/*
 * CRF_StdSegNStateNode_WithoutDurLab::computeTransMatrix
 *
 * Computes the log of the state vector (stateArray) and the log of the transition matrix (transMatrix)
 *   and stores them as appropriate
 */
double CRF_StdSegNStateNode_WithoutDurLab::computeTransMatrix()
{
	checkNumPrevNodes();

	double result=0.0;

	double* lambda = this->crf_ptr->getLambda();
	float* seg_ftr_buf = this->ftrBuf;
//	QNUInt32 clab = 0;
	// Note: it should be this->numPrevNodes instead of this->nodeLabMaxDur as the number of iterations of the outer loop.
	// It's because numPrevNodes is enough for transition calculation while nodeLabMaxDur might be larger than numPrevNodes for some nodes.
	for (QNUInt32 dur = 1; dur <= this->numPrevNodes; dur++)
	{
		CRF_StateNode* prevAdjacentSeg = this->prevNodes[this->numPrevNodes - dur];
		double* stateArrayForCurDur = &(this->stateArray[this->nActualLabs * (dur - 1)]);
//		double* transMatrixForCurDur = &(this->transMatrix[this->nActualLabs * this->nActualLabs * (dur - 1)]);
		double* denseTransMatrixForCurDur = &(this->denseTransMatrix[this->nFullLabs * this->nFullLabs * (dur - 1)]);
		double* diagTransMatrixForCurDur = &(this->diagTransMatrix[this->nLabs * (dur - 1)]);
		double* offDiagTransMatrixForCurDur = &(this->offDiagTransMatrix[this->nLabs * (dur - 1)]);
		for (QNUInt32 lab = 0; lab < this->nActualLabs; lab++)
		{
			// pass lab to computeStateArrayValue() instead of clab
			//this->stateArray[clab]=this->crf_ptr->getFeatureMap()->computeStateArrayValue(seg_ftr_buf,lambda,clab);
			stateArrayForCurDur[lab]=this->crf_ptr->getFeatureMap()->computeStateArrayValue(seg_ftr_buf,lambda,lab);

			// All entries on the diagonal get their self transition assigned
			diagTransMatrixForCurDur[lab]=this->crf_ptr->getFeatureMap()->computeTransMatrixValue(seg_ftr_buf,lambda,lab,lab);

			// Besides the diagonal, we need to update the off-diagonal or the dense transition matrix
			// dense transition matrix is updated when the current label is a start state (e.g. when the
			// lab % nStates == 0
			if (lab % this->nStates == 0) {
				for (QNUInt32 plab = 0; plab < this->nFullLabs; plab++) {
					// Our index into the dense transition matrix needs to be munged a bit
					QNUInt32 idx = plab * nFullLabs + lab / this->nStates;
					// And our previous label is actually the end state for the previous "label"
					QNUInt32 real_plab = plab * nStates + nStates - 1;  // real_plab is the end state
					denseTransMatrixForCurDur[idx] = this->crf_ptr->getFeatureMap()->computeTransMatrixValue(seg_ftr_buf,lambda,real_plab,lab);
				}
			}
			else {
				// We're on the off-diagonal - lab is not a start  state and the only previous
				// transition must be from the immediate prior state (e.g. lab-1)
				offDiagTransMatrixForCurDur[lab-1] = this->crf_ptr->getFeatureMap()->computeTransMatrixValue(seg_ftr_buf,lambda,lab-1,lab);
			}


//			//TODO: for (QNUInt32 plab=0; plab<nLabs_of_prevNode; plab++) {
//			//for (QNUInt32 plab=0; plab<nLabs; plab++) {
//			for (QNUInt32 plab = 0; plab < prevAdjacentSeg->getNumAvailLabs(); plab++) {
//
//				//QNUInt32 idx = plab * this->nLabs + clab;
//				QNUInt32 idx = plab * this->nActualLabs + lab;
//				//TODO: design a feature map in which the transition matrix calculation can take different dimensions of plab (from prevNode) and clab (from current node).
//				//this->transMatrix[idx]=this->crf_ptr->getFeatureMap()->computeTransMatrixValue(seg_ftr_buf,lambda,plab,clab);
//				transMatrixForCurDur[idx]=this->crf_ptr->getFeatureMap()->computeTransMatrixValue(seg_ftr_buf,lambda,plab,lab);
//			}
//			clab++;

		}
		seg_ftr_buf += this->nFtrsPerSeg;
	}
	// These are the cases when the current node serves as the beginning segment of the sequence, so there is no previous node.
	for (QNUInt32 dur = this->numPrevNodes + 1; dur <= this->nodeLabMaxDur; dur++)
	{
		double* stateArrayForCurDur = &(this->stateArray[this->nActualLabs * (dur - 1)]);
		for (QNUInt32 lab = 0; lab < this->nActualLabs; lab++)
		{
			//this->stateArray[clab]=this->crf_ptr->getFeatureMap()->computeStateArrayValue(seg_ftr_buf,lambda,clab);
			stateArrayForCurDur[lab]=this->crf_ptr->getFeatureMap()->computeStateArrayValue(seg_ftr_buf,lambda,lab);

//			clab++;
		}
		seg_ftr_buf += this->nFtrsPerSeg;
	}
	return result;
}

/*
 * CRF_StdSegNStateNode_WithoutDurLab::computeAlpha
 *
 * Read alpha vectors of previous nodes directly from prevNode and store the result of the alpha vector in alphaArray.
 *
 * Stub function.
 * Should compute the alpha vector for the forward backward computation for this node.
 */
double CRF_StdSegNStateNode_WithoutDurLab::computeAlpha()
{
	//QNUInt32 nLabs = this->crf_ptr->getNLabs();
	this->alphaScale=0.0;

	checkNumPrevNodes();

//	double* tempAlphaArray = new double[this->nodeLabMaxDur];

	try {
//		QNUInt32 clab = 0;
		for (QNUInt32 lab = 0; lab < this->nActualLabs; lab++)
		{
//			double maxv = LOG0;
			for (QNUInt32 dur = 1; dur <= this->numPrevNodes; dur++)
			{
				CRF_StateNode* prevAdjacentSeg = this->prevNodes[this->numPrevNodes - dur];
				double* prev_adj_seg_alpha = prevAdjacentSeg->getAlpha();
				double* stateArrayForCurDur = &(this->stateArray[this->nActualLabs * (dur - 1)]);
				double* denseTransMatrixForCurDur = &(this->denseTransMatrix[this->nFullLabs * this->nFullLabs * (dur - 1)]);
				double* diagTransMatrixForCurDur = &(this->diagTransMatrix[this->nLabs * (dur - 1)]);
				double* offDiagTransMatrixForCurDur = &(this->offDiagTransMatrix[this->nLabs * (dur - 1)]);

				QNUInt32 durAlphaArrayId = lab * this->nodeLabMaxDur + dur - 1;
				QNUInt32 logAddID = 0;
				double maxv;

				// Compute the self transition - all labels get this
//				this->alphaArray_WithDur[durAlphaArrayId] = prev_adj_seg_alpha[lab] + diagTransMatrixForCurDur[lab];
				this->logAddAcc[logAddID] = prev_adj_seg_alpha[lab] + diagTransMatrixForCurDur[lab];
				maxv = this->logAddAcc[logAddID];
				logAddID++;

				if (lab % this->nStates == 0)
				{
					// Here lab is a new start state, so all end state transitions to it must be computed
					QNUInt32 dense_clab = lab / this->nStates; //Used to index into dense transition matrix

//					this->logAddAcc[0] = prev_adj_seg_alpha[this->nStates - 1] + denseTransMatrixForCurDur[0 * this->nFullLabs + dense_clab];
//					logAddID++;
//					double maxv=this->logAddAcc[0];

					for (QNUInt32 dense_plab = 0; dense_plab < this->nFullLabs; dense_plab++) {
						QNUInt32 real_plab = dense_plab * this->nStates + this->nStates - 1;
						QNUInt32 dense_idx = dense_plab * nFullLabs + dense_clab;
						this->logAddAcc[logAddID] = prev_adj_seg_alpha[real_plab] + denseTransMatrixForCurDur[dense_idx];

						if (this->logAddAcc[logAddID] > maxv) {
							maxv = logAddAcc[logAddID];
						}
						logAddID++;
					}
//					double logSum = logAdd(this->logAddAcc, maxv, logAddID);
//					this->alphaArray_WithDur[durAlphaArrayId] = logAdd(this->alphaArray_WithDur[durAlphaArrayId], logSum);
				}
				else {
					// Here lab is an interior state, so only transitions from the previous state need to be accounted for
//					double tmpAlpha = prev_adj_seg_alpha[lab - 1] + diagTransMatrixForCurDur[lab - 1];
//					this->alphaArray_WithDur[durAlphaArrayId] = logAdd(this->alphaArray_WithDur[durAlphaArrayId], tmpAlpha);
					this->logAddAcc[logAddID] = prev_adj_seg_alpha[lab - 1] + diagTransMatrixForCurDur[lab - 1];
					if (this->logAddAcc[logAddID] > maxv) {
						maxv = logAddAcc[logAddID];
					}
					logAddID++;
				}

				this->alphaArray_WithDur[durAlphaArrayId] = logAdd(this->logAddAcc, maxv, logAddID); // log-summing previous alphas and transition values over all paths to lab with dur
				//this->alphaArray[clab]+=this->stateArray[clab];
				this->alphaArray_WithDur[durAlphaArrayId] += stateArrayForCurDur[lab]; // add the state value for lab with dur

//				clab++;
			}
			// These are the cases when the current node serves as the beginning segment of the sequence, so there is no previous node.
			for (QNUInt32 dur = this->numPrevNodes + 1; dur <= this->nodeLabMaxDur; dur++)
			{
				double* stateArrayForCurDur = &(this->stateArray[this->nActualLabs * (dur - 1)]);
				//this->alphaArray[clab]=this->stateArray[clab];
				this->alphaArray_WithDur[lab*this->nodeLabMaxDur + dur - 1] = stateArrayForCurDur[lab];

//				clab++;
			}

			this->alphaArray[lab] = logAdd(&(this->alphaArray_WithDur[lab*this->nodeLabMaxDur]),this->nodeLabMaxDur);
		}
	}
	catch (exception &e) {
		string errstr="CRF_StdSegNStateNode_WithoutDurLab::computeAlpha() caught exception: "+string(e.what())+" while computing alpha";
		throw runtime_error(errstr);
		return(-1);
	}

//	delete [] tempAlphaArray;
	return this->alphaScale;
}

/*
 * CRF_StdSegNStateNode_WithoutDurLab::computeFirstAlpha
 *
 * Stub function.
 * Should compute the alpha vector for this node for the special case where the node is the first
 * node in the sequence.
 */
double CRF_StdSegNStateNode_WithoutDurLab::computeFirstAlpha()
{
	//QNUInt32 nLabs = this->crf_ptr->getNLabs();
	this->alphaScale=0.0;

	//nodeMaxLab for the first node of the sequence is usually equal to nActualLabs (since nodeLabMaxDur==1).
	for (QNUInt32 lab = 0; lab < this->numAvailLabs; lab++)
	{
		//this->alphaArray[clab]+=this->stateArray[clab];
		this->alphaArray_WithDur[lab] = this->stateArray[lab];
		this->alphaArray[lab]=this->stateArray[lab];
	}
	return this->alphaScale;
}

/*
 * CRF_StdSegNStateNode_WithoutDurLab::computeBeta
 *
 * Inputs: scale - scaling constant for result_beta array
 *
 * Returns:
 *
 * Read the beta vectors of next nodes directly from nextNode and store the result of the beta vector in betaArray.
 *
 * Stub function.
 * Should compute the beta vector for the node before this one and store it in result_beta
 */
double CRF_StdSegNStateNode_WithoutDurLab::computeBeta(double scale)
{
	// Logic desired:
	//	* Compute beta_i[size of alpha[]+1] to be all 1s
	//	* Multiply M_i[current] by beta_i[current+1] to get beta_i[current]

//	QNUInt32 nLabs = this->crf_ptr->getNLabs();
//
//	for (QNUInt32 clab=0; clab<nLabs; clab++) {
//		this->tempBeta[clab]=this->betaArray[clab]+this->stateArray[clab];
//	}
//
//	for (QNUInt32 plab=0; plab<nLabs; plab++) {
//		this->logAddAcc[0]=this->transMatrix[plab*nLabs+0]+this->tempBeta[0];
//		double maxv=this->logAddAcc[0];
//		for (QNUInt32 clab=1; clab<nLabs; clab++) {
//			this->logAddAcc[clab]=this->transMatrix[plab*nLabs+clab]+this->tempBeta[clab];
//			if (this->logAddAcc[clab]>maxv) {
//				maxv=this->logAddAcc[clab];
//			}
//		}
//		try {
//			result_beta[plab]=logAdd(this->logAddAcc,maxv,nLabs);
//		}
//		catch (exception &e) {
//			string errstr="CRF_StdSegStateNode::computeBeta() caught exception: "+string(e.what())+" while computing beta";
//			throw runtime_error(errstr);
//			return(-1);
//		}
//	}

	checkNumNextNodes();

	// if numNextNodes == 0, this is the last node of the sequence.
	// Sets the beta value in this node to the special case for the end of the sequence.
	if (this->numNextNodes == 0)
	{
		setTailBeta();
		return this->alphaScale;
	}

//	QNUInt32 nextlab = 0;
//	for (QNUInt32 dur = 1; dur <= this->numNextNodes; dur++)
//	{
//		CRF_StateNode* nextAdjacentSeg = this->nextNodes[dur - 1];
//		double* next_adj_seg_beta = nextAdjacentSeg->getBeta();
//		for (QNUInt32 lab = 0; lab < this->nActualLabs; lab++) {
//			this->tempBeta[nextlab] = next_adj_seg_beta[nextlab] + nextAdjacentSeg->getStateValue(nextlab);
//
//			// just for debugging
////			cout << "tempBeta[" << nextlab << "]=" << "next_seg_beta[" << nextlab << "](" << next_adj_seg_beta[nextlab] << ") + next_seg_state_value[" << nextlab << "](" << nextAdjacentSeg->getStateValue(nextlab) << ")=" << this->tempBeta[nextlab] << endl;
//
//			nextlab++;
//		}
//	}

//	QNUInt32 nextlab = 0;
	for (QNUInt32 dur = 1; dur <= this->numNextNodes; dur++)
	{
		CRF_StateNode* nextAdjacentSeg = this->nextNodes[dur - 1];
		double* next_adj_seg_beta = nextAdjacentSeg->getBeta();

//		double* stateArrayForCurDur = &(this->stateArray[this->nActualLabs * (dur - 1)]);
		double* tempBetaForNextDur = &(this->tempBeta[this->nActualLabs * (dur - 1)]);
		for (QNUInt32 lab = 0; lab < this->nActualLabs; lab++)
		{
			//this->tempBeta[nextlab] = next_adj_seg_beta[nextlab] + nextAdjacentSeg->getStateValue(nextlab);
			tempBetaForNextDur[lab] = next_adj_seg_beta[lab] + nextAdjacentSeg->getStateValue(lab,dur);
//			this->tempBeta[this->nActualLabs * (dur - 1) + lab] = next_adj_seg_beta[lab] + nextAdjacentSeg->getStateValue(lab,dur);
//
			//nextlab++;
		}
	}


	for (QNUInt32 clab = 0; clab < this->numAvailLabs; clab++)
	{
		CRF_StateNode* nextAdjacentSeg = this->nextNodes[0];

		QNUInt32 logAddID = 0;
		double maxv;

		// To initialize maxv, we pre-compute the first value in logAddAcc. This value would be computed again below.
		// Compute the self transition - all labels get this
		this->logAddAcc[0] = nextAdjacentSeg->getTransValue(clab, clab, 1) + this->tempBeta[clab];
//		this->logAddAcc[logAddID] = prev_adj_seg_alpha[lab] + diagTransMatrixForCurDur[lab];
		maxv = this->logAddAcc[0];

//		//this->logAddAcc[0] = nextAdjacentSeg->getTransValue(clab, 0) + this->tempBeta[0];
////		this->logAddAcc[0] = nextAdjacentSeg->getTransValue(clab, 0, 1) + nextAdjacentSeg->getTempBeta(0, 1);
//		this->logAddAcc[0] = nextAdjacentSeg->getTransValue(clab, 0, 1) + this->tempBeta[0];
//		double maxv=this->logAddAcc[0];
////		QNUInt32 nextlab = 0;
//		QNUInt32 logAddID = 0;

		for (QNUInt32 dur = 1; dur <= this->numNextNodes; dur++)
		{
			nextAdjacentSeg = this->nextNodes[dur - 1];
			double* tempBetaForNextDur = &(this->tempBeta[this->nActualLabs * (dur - 1)]);

			// Compute the self transition - all labels get this
			this->logAddAcc[logAddID] = nextAdjacentSeg->getTransValue(clab, clab, dur) + tempBetaForNextDur[clab];
			if (this->logAddAcc[logAddID] > maxv) {
				maxv = this->logAddAcc[logAddID];
			}
			logAddID++;

			if ((clab + 1) % this->nStates == 0)
			{
				// Here clab is an end state, so all new start state transitions from it must be computed
				for (QNUInt32 dense_next_lab = 0; dense_next_lab < this->nFullLabs; dense_next_lab++) {
					QNUInt32 real_next_lab = dense_next_lab * this->nStates;
					this->logAddAcc[logAddID] = nextAdjacentSeg->getTransValue(clab, real_next_lab, dur) + tempBetaForNextDur[real_next_lab];

//					if (this->logAddAcc[nextlab]>maxv) {
//						maxv=logAddAcc[nextlab];
//					}
//					nextlab++;
					if (this->logAddAcc[logAddID] > maxv) {
						maxv = this->logAddAcc[logAddID];
					}
					logAddID++;
				}
			}
			else {
				// Here clab is an interior state, so only transitions to the next state need to be accounted for
				this->logAddAcc[logAddID] = nextAdjacentSeg->getTransValue(clab, clab + 1, dur) + tempBetaForNextDur[clab + 1];
				if (this->logAddAcc[logAddID] > maxv) {
					maxv = this->logAddAcc[logAddID];
				}
				logAddID++;
			}
		}
		try {
			this->betaArray[clab] = logAdd(this->logAddAcc, maxv, logAddID);
		}
		catch (exception &e) {
			string errstr="CRF_StdSegNStateNode_WithoutDurLab::computeBeta() caught exception: "+string(e.what())+" while computing beta";
			throw runtime_error(errstr);
			return(-1);
		}
	}

	// calculate tempBeta on the current node for later use in the beta calculation of previous nodes.
//	for (QNUInt32 dur = 1; dur <= this->nodeLabMaxDur; dur++)
//	{
//		double* stateArrayForCurDur = &(this->stateArray[this->nActualLabs * (dur - 1)]);
//		double* tempBetaForCurDur = &(this->tempBeta[this->nActualLabs * (dur - 1)]);
//		for (QNUInt32 lab = 0; lab < this->nActualLabs; lab++)
//		{
//			tempBetaForCurDur[lab] = this->betaArray[lab] + stateArrayForCurDur[lab];
//		}
//	}

	return this->alphaScale;
}

/*
 * CRF_StdSegNStateNode_WithoutDurLab::computeExpF
 *
 * Inputs: *ExpF - vector to store expected values of feature functions
 *         *grad - vector to store computed gradient values
 *         Zx - normalization constant
 *         prev_lab - previous node label (transition feature ExpF computation)
 *
 * Returns:
 *
 * Read alpha vectors of previous nodes directly from prevNode for use in transition feature ExpF computation.
 *
 * Stub function.
 * Should compute gradient and expected values for features in this node and store them in *grad and
 *   *ExpF vectors respectively.  State features and transition features are computed in the same function.
 */
double CRF_StdSegNStateNode_WithoutDurLab::computeExpF(double* ExpF, double* grad, double Zx, QNUInt32 prev_lab)
{
	string errstr="CRF_StdSegNStateNode_WithoutDurLab::computeExpF() threw exception: This function has not been implemented yet!";
	throw runtime_error(errstr);

//	checkNumPrevNodes();
//
//	QNUInt32 actualLab = this->label;
//	QNUInt32 labDur = CRF_LAB_BAD;
//	QNUInt32 actualPLab = prev_lab;
//	QNUInt32 plabDur = CRF_LAB_BAD;
//
//	if (actualLab != CRF_LAB_BAD)
//	{
//		if (actualLab >= this->nActualLabs * this->labMaxDur)
//		{
//			string errstr="CRF_StdSegNStateNode_WithoutDurLab::computeExpF() threw exception: the label is larger than "+ stringify(this->nActualLabs * this->labMaxDur);
//			throw runtime_error(errstr);
//		}
//		actualLab = this->label % this->nActualLabs;
//		labDur = this->label / this->nActualLabs + 1;
//	}
//	if (actualPLab != CRF_LAB_BAD)
//	{
//		if (actualPLab >= this->nActualLabs * this->labMaxDur)
//		{
//			string errstr="CRF_StdSegNStateNode_WithoutDurLab::computeExpF() threw exception: the previous label is larger than "+ stringify(this->nActualLabs * this->labMaxDur);
//			throw runtime_error(errstr);
//		}
//		actualPLab = prev_lab % this->nActualLabs;
//		plabDur = prev_lab / this->nActualLabs + 1;
//	}
//
//	double logLi=0.0;
//	double alpha_beta=0.0;
//	//QNUInt32 nLabs = this->crf_ptr->getNLabs();
//
//	double* lambda = this->crf_ptr->getLambda();
//	double alpha_beta_tot = 0.0;
//	double alpha_beta_trans_tot=0.0;
//	float* seg_ftr_buf = this->ftrBuf;
////	QNUInt32 clab = 0;
//	for (QNUInt32 dur = 1; dur <= this->numPrevNodes; dur++)
//	{
//		CRF_StateNode* prevAdjacentSeg = this->prevNodes[this->numPrevNodes - dur];
//		double* prev_adj_seg_alpha = prevAdjacentSeg->getAlpha();
//		double* stateArrayForCurDur = &(this->stateArray[this->nActualLabs * (dur - 1)]);
//		double* transMatrixForCurDur = &(this->transMatrix[this->nActualLabs * this->nActualLabs * (dur - 1)]);
//
//		for (QNUInt32 lab = 0; lab < this->nActualLabs; lab++)
//		{
//			//alpha_beta=expE(this->alphaArray[clab]+this->betaArray[clab]-Zx);
//			alpha_beta=expE(this->alphaArray_WithDur[lab * this->nodeLabMaxDur + dur - 1] + this->betaArray[lab] - Zx);
//			alpha_beta_tot += alpha_beta;
//			//bool match=(clab==this->label);
//			bool match=(lab==actualLab && dur==labDur);
//
//			// just for debugging
////			if (match)
//				//cout << clab << " ";
////				cout << "state label match: actualLab=" << actualLab << ", duration=" << labDur << endl;
//
//			//logLi+=this->crf_ptr->getFeatureMap()->computeStateExpF(seg_ftr_buf,lambda,ExpF,grad,alpha_beta,this->label,clab);
//			if (match)
//			{
//				logLi+=this->crf_ptr->getFeatureMap()->computeStateExpF(seg_ftr_buf,lambda,ExpF,grad,alpha_beta,actualLab,lab);
//			} else {
//				logLi+=this->crf_ptr->getFeatureMap()->computeStateExpF(seg_ftr_buf,lambda,ExpF,grad,alpha_beta,CRF_LAB_BAD,lab);
//			}
////			//TODO: verify: this->nLabs or nLabs_of_prevNode (probably this)
////			if (prev_lab > this->nLabs) {
////				// if prev_lab > nLabs, we're in the first label frame and there are no previous
////				// transitions - skip the transition calculation in this case
////				// but set the alpha_beta_trans_tot to 1.0 for the check below
////				alpha_beta_trans_tot=1.0;
////			}
////			else {
//				// Otherwise do the transition calculations
//				//for (QNUInt32 plab=0; plab<nLabs; plab++) {
//				for (QNUInt32 plab = 0; plab < prevAdjacentSeg->getNumAvailLabs(); plab++) {
//
//					//QNUInt32 idx = plab * this->nLabs + clab;
//					QNUInt32 idx = plab * this->nActualLabs + lab;
//					//alpha_beta=expE(prev_adj_seg_alpha[plab]+this->transMatrix[idx]+this->stateArray[clab]+this->betaArray[clab]-Zx);
//					alpha_beta=expE(prev_adj_seg_alpha[plab]+transMatrixForCurDur[idx]+stateArrayForCurDur[lab]+this->betaArray[lab]-Zx);
//					alpha_beta_trans_tot+=alpha_beta;
//					//match=((clab==this->label)&&(plab==prev_lab));
//					match=(lab==actualLab && dur==labDur && plab==actualPLab);
//
//					// just for debugging
////					if (match)
//						//cout << "(" << clab << "<-" <<  plab << ") ";
////						cout << "transition label match: " << "(" << actualLab << "<-" <<  actualPLab << ") " << endl;
//
//					// TODO: design a feature map in which the transition matrix calculation can take different size of plab (from prevNode) and clab (from current node).
//					//logLi+=this->crf_ptr->getFeatureMap()->computeTransExpF(seg_ftr_buf,lambda,ExpF,grad,alpha_beta,prev_lab,this->label,plab,clab);
//					if (match)
//					{
//						logLi+=this->crf_ptr->getFeatureMap()->computeTransExpF(seg_ftr_buf,lambda,ExpF,grad,alpha_beta,actualPLab,actualLab,plab,lab);
//					} else {
//						logLi+=this->crf_ptr->getFeatureMap()->computeTransExpF(seg_ftr_buf,lambda,ExpF,grad,alpha_beta,CRF_LAB_BAD,CRF_LAB_BAD,plab,lab);
//					}
//				}
////			}
////			clab++;
//		}
//		seg_ftr_buf += this->nFtrsPerSeg;
//	}
//	// These are the cases when the current node serves as the beginning segment of the sequence, so there is no previous node.
//	for (QNUInt32 dur = this->numPrevNodes + 1; dur <= this->nodeLabMaxDur; dur++)
//	{
//		for (QNUInt32 lab = 0; lab < this->nActualLabs; lab++)
//		{
//			// just debugging
////			cout << "Second phase:: numPrevNodes: " << numPrevNodes << ", dur: " << dur << ", lab: " << lab << endl;
//
//			//alpha_beta=expE(this->alphaArray[clab]+this->betaArray[clab]-Zx);
//			alpha_beta=expE(this->alphaArray_WithDur[lab * this->nodeLabMaxDur + dur - 1] + this->betaArray[lab] - Zx);
//			alpha_beta_tot += alpha_beta;
//			//bool match=(clab==this->label);
//			bool match=(lab==actualLab && dur==labDur);
//
//			// just for debugging
////			cout << "alpha_beta=" << alpha_beta << ", alpha_beta_tot=" << alpha_beta_tot << endl;
//
//			// just for debugging
////			if (match)
//				//cout << clab << " ";
////				cout << "state label match: actualLab=" << actualLab << ", duration=" << labDur << endl;
//
//
//			//logLi+=this->crf_ptr->getFeatureMap()->computeStateExpF(seg_ftr_buf,lambda,ExpF,grad,alpha_beta,this->label,clab);
//			if (match)
//			{
//				logLi+=this->crf_ptr->getFeatureMap()->computeStateExpF(seg_ftr_buf,lambda,ExpF,grad,alpha_beta,actualLab,lab);
//			} else {
//				logLi+=this->crf_ptr->getFeatureMap()->computeStateExpF(seg_ftr_buf,lambda,ExpF,grad,alpha_beta,CRF_LAB_BAD,lab);
//			}
//
////			clab++;
//		}
//		seg_ftr_buf += this->nFtrsPerSeg;
//	}
//
//	if (this->numPrevNodes == 0)
//	{
//		// if numPrevNodes == 0, there are no previous transitions
//		// - skip the transition calculation in this case
//		// but set the alpha_beta_trans_tot to 1.0 for the check below
//		alpha_beta_trans_tot=1.0;
//	}
//
//	//just for debugging
////	cout << "\tAlpha_beta_tot: " << alpha_beta_tot << "\tAlpha_beta_trans_tot: " << alpha_beta_trans_tot << endl;
//
//	// alpha_beta_tot and alpha_beta_trans_tot are no longer equal to 1 but less than 1 except the ending node.
//	if ((alpha_beta_tot >1.000001))  {
//		string errstr="CRF_StdSegNStateNode_WithoutDurLab::computeExpF() threw exception: Probability sums greater than 1.0 "+stringify(alpha_beta_tot);
//		throw runtime_error(errstr);
//	}
////	else if (alpha_beta_tot < 0.999999) {
//	else if (alpha_beta_tot < -0.000001) {
////		string errstr="CRF_StdSegStateNode::computeExpF() threw exception: Probability sums less than 1.0 "+stringify(alpha_beta_tot);
//		string errstr="CRF_StdSegNStateNode_WithoutDurLab::computeExpF() threw exception: Probability sums less than 0.0 "+stringify(alpha_beta_tot);
//		throw runtime_error(errstr);
//	}
//	else if (alpha_beta_trans_tot > 1.000001) {
//		string errstr="CRF_StdSegNStateNode_WithoutDurLab::computeExpF() threw exception: Trans Probability sums greater than 1.0 "+stringify(alpha_beta_trans_tot);
//		throw runtime_error(errstr);
//	}
////	else if (alpha_beta_trans_tot < 0.999999) {
//	else if (alpha_beta_trans_tot < -0.000001) {
////		string errstr="CRF_StdSegStateNode::computeExpF() threw exception: Trans Probability sums less than 1.0 "+stringify(alpha_beta_trans_tot);
//		string errstr="CRF_StdSegNStateNode_WithoutDurLab::computeExpF() threw exception: Trans Probability sums less than 0.0 "+stringify(alpha_beta_trans_tot);
//		throw runtime_error(errstr);
//	}
//
//	// just for debugging
////	cout << endl;
//
//	return logLi;
}



// Added by Ryan
/*
 * CRF_StdSegNStateNode_WithoutDurLab::getTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab, QNUInt32 cur_dur)
 *
 * Correct getTransValue() function for CRF_StdSegNStateNode_WithoutDurLab.
 *
 */
double CRF_StdSegNStateNode_WithoutDurLab::getTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab, QNUInt32 cur_dur)
{
	if ((cur_lab % this->nStates == 0) && ((prev_lab + 1) % this->nStates == 0)) {
		QNUInt32 cur_idx = cur_lab / this->nStates;
		QNUInt32 prev_idx = (prev_lab + 1) / this->nStates - 1;
		QNUInt32 idx = this->nFullLabs * this->nFullLabs * (cur_dur - 1) + prev_idx * this->nFullLabs + cur_idx;
		return this->denseTransMatrix[idx];
	}
	else if (cur_lab == prev_lab) {
		return this->diagTransMatrix[cur_lab];
	}
	else if (cur_lab == prev_lab + 1) {
		return this->offDiagTransMatrix[cur_lab - 1];
	}
	else {
		// Changed by Ryan
		// In theory, it should return LOG0 instead of 0.
		// But we would report error here instead.
		//return 0;
		string errstr="CRF_StdNStateNode::getTransValue() threw exception: Invalid transition from previous lab (="
				+ stringify(prev_lab) + ") to current lab (=" + stringify(cur_lab) + ").";
		throw runtime_error(errstr);
	}
}

// Added by Ryan
/*
 * CRF_StdSegNStateNode_WithoutDurLab::getStateValue(QNUInt32 cur_lab, QNUInt32 cur_dur)
 *
 * Correct getStateValue() function for CRF_StdSegNStateNode_WithoutDurLab.
 *
 */
double CRF_StdSegNStateNode_WithoutDurLab::getStateValue(QNUInt32 cur_lab, QNUInt32 cur_dur)
{
	return this->stateArray[this->nActualLabs * (cur_dur - 1) + cur_lab];
}

// Added by Ryan
/*
 * CRF_StdSegNStateNode_WithoutDurLab::getFullTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab, QNUInt32 cur_dur)
 *
 * Correct getFullTransValue() function for CRF_StdSegNStateNode_WithoutDurLab.
 *
 */
double CRF_StdSegNStateNode_WithoutDurLab::getFullTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab, QNUInt32 cur_dur)
{
	return getTransValue(prev_lab, cur_lab, cur_dur) + getStateValue(cur_lab, cur_dur);
}

// Added by Ryan
/*
 * CRF_StdSegNStateNode_WithoutDurLab::getTempBeta(QNUInt32 next_lab, QNUInt32 next_dur)
 *
 */
double CRF_StdSegNStateNode_WithoutDurLab::getTempBeta(QNUInt32 next_lab, QNUInt32 next_dur)
{
	return this->tempBeta[this->nActualLabs * (next_dur - 1) + next_lab];
}

// Disable all these functions by overriding them with exception handling. Use their modified version below.
// Added by Ryan
/*
 * CRF_StdSegNStateNode_WithoutDurLab::getTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab)
 *
 * Disabled in CRF_StdSegNStateNode_WithoutDurLab, use getTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab, QNUInt32 dur) instead.
 *
 */
double CRF_StdSegNStateNode_WithoutDurLab::getTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab)
{
	string errstr="Error: use CRF_StdSegNStateNode_WithoutDurLab::getTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab, QNUInt32 dur) instead.";
	throw runtime_error(errstr);
}

// Added by Ryan
/*
 * CRF_StdSegNStateNode_WithoutDurLab::getStateValue(QNUInt32 cur_lab)
 *
 * Disabled in CRF_StdSegNStateNode_WithoutDurLab, use getStateValue(QNUInt32 cur_lab, QNUInt32 dur) instead.
 *
 */
double CRF_StdSegNStateNode_WithoutDurLab::getStateValue(QNUInt32 cur_lab)
{
	string errstr="Error: use CRF_StdSegNStateNode_WithoutDurLab::getStateValue(QNUInt32 cur_lab, QNUInt32 dur) instead.";
	throw runtime_error(errstr);
}

// Added by Ryan
/*
 * CRF_StdSegNStateNode_WithoutDurLab::getFullTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab)
 *
 * Disabled in CRF_StdSegNStateNode_WithoutDurLab, use getFullTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab, QNUInt32 dur) instead.
 *
 */
double CRF_StdSegNStateNode_WithoutDurLab::getFullTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab)
{
	string errstr="Error: use CRF_StdSegNStateNode_WithoutDurLab::getFullTransValue(QNUInt32 prev_lab, QNUInt32 cur_lab, QNUInt32 dur) instead.";
	throw runtime_error(errstr);
}

