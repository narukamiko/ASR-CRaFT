/*
 * CRF_NewGradBuilder.cpp
 *
 * Copyright (c) 2010
 * Author: Jeremy Morris
 *
 */
#include "CRF_NewGradBuilder.h"

/*
 * CRF_NewGradBuilder constructor
 *
 * See CRF_GradBuilder constructor for details
 */
CRF_NewGradBuilder::CRF_NewGradBuilder(CRF_Model* crf_in)
	: CRF_GradBuilder(crf_in)
{
	for (QNUInt32 i=0; i<this->num_labs; i++) {
		this->alpha_base[i]=0.0;
	}
	this->lambda_len = crf_in->getLambdaLen();
	this->ExpF = new double[this->lambda_len];
	this->nodeList = new CRF_StateVector();
}

/*
 * CRF_NewGradBuilder destructor
 *
 */
CRF_NewGradBuilder::~CRF_NewGradBuilder()
{
	cerr << "newgradbuilder destructor" << endl;
	//if (this->ftr_buf != NULL) { delete this->ftr_buf; }
	//if (this->lab_buf != NULL) { delete this->lab_buf; }
	delete [] this->ExpF;
}

/*
 * CRF_NewGradBuilder::buildGradient
 *
 * Input: *ftr_stream - input feature stream
 *        *grad - gradient vector return value
 *        *Zx_out - normalization constant return value
 *
 * Computes the gradient given the current CRF model and the features in ftr_strm and returns it in
 * grad.  Zx_out contains the normalization constant for the current sequence.
 */
double CRF_NewGradBuilder::buildGradient(CRF_FeatureStream* ftr_strm, double* grad, double* Zx_out)
{
	QNUInt32 lambda_len = this->crf->getLambdaLen();

	double logLi = 0.0;

	// Changed by Ryan, CRF_InFtrStream_SeqMultiWindow can just accept 1 as bunch_size for frame model.
//	size_t bunch_size = 3;
	size_t bunch_size = 1;

	size_t num_ftrs=ftr_strm->num_ftrs();

	// Added by Ryan
//	QNUInt32 lab_max_dur = this->crf->getLabMaxDur();
//	QNUInt32 nActualLabs = this->crf->getNActualLabs();

	if (this->ftr_buf==NULL) {  // First pass through initialize the buffers
		// Changed by Ryan
		// - Note: They are newed only when buildGradient() is called first time,
		// and they are deleted only when the CRF_NewGradBuilder object is destructed.
		this->ftr_buf = new float[num_ftrs*bunch_size];
		this->lab_buf = new QNUInt32[bunch_size];
//		size_t labs_width = ftr_strm->num_labs();
//		QNUInt32 ftr_buf_size = num_ftrs * lab_max_dur;
//		//QNUInt32 lab_buf_size = labs_width * lab_max_dur;
//		QNUInt32 lab_buf_size = labs_width;
//		this->ftr_buf = new float[ftr_buf_size];
//		this->lab_buf = new QNUInt32[lab_buf_size];
	}

	size_t ftr_count;

	for (QNUInt32 i=0; i<lambda_len; i++) {
		this->ExpF[i]=0.0;
	}

	// Changed by Ryan
	QNUInt32 nodeCnt=0;
	do {
		// First, read in the next training value from the file
		//	We can read in a "bunch" at a time, then separate them into individual frames
		ftr_count=ftr_strm->read(bunch_size,this->ftr_buf,this->lab_buf);

		for (QNUInt32 i=0; i<ftr_count; i++) {
			//cout << "\tLabel: " << lab_buf[i] << "\tFeas:";
			// Now, separate the bunch into individual frames
			float* new_buf = new float[num_ftrs];
			for (QNUInt32 j=0; j<num_ftrs; j++) {
				int idx=i*num_ftrs+j;
				new_buf[j]=this->ftr_buf[idx];
				//cout << " " << new_buf[j];
			}
			//cout << endl;
			// Store the current frame/label information in a sequence node
			//	* sequence nodes create a doubly-linked list, with the previous node known at creation time
			//  * new_buf will be deleted when this sequence object gets deleted
			/*if (nodeCnt >= this->nodeList->size() ) {
				this->nodeList.push_back(new CRF_StdStateNodeLog(new_buf,num_ftrs,this->lab_buf[i],this->crf));
			}
			else {
				this->nodeList.at(nodeCnt)->reset(new_buf,num_ftrs,this->lab_buf[i],this->crf);
			}*/
			this->nodeList->set(nodeCnt,new_buf,num_ftrs,this->lab_buf[i],this->crf);
			//cout << "Label: " << this->lab_buf[i] << endl;
			double value=this->nodeList->at(nodeCnt)->computeTransMatrix();
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
			//scale=this->nodeList->at(nodeCnt)->computeAlpha(prev_alpha);

			//double sum = this->nodeList.at(nodeCnt)->computeAlphaSum();
			//double* alpha=this->nodeList.at(nodeCnt)->getAlpha();

			logLi-=scale;
			nodeCnt++;
			// End of Loop:
			//	alpha[i] = alpha[i-1]*M[i]
		}
	} while (ftr_count >= bunch_size);

//	QNUInt32 nodeCnt=0;
//	do {
//		// First, read in the next training value from the file
//		//	We can read in a "bunch" at a time, then separate them into individual frames
//		ftr_count=ftr_strm->read(bunch_size,this->ftr_buf,this->lab_buf);
//
//		if (ftr_count > 0)
//		{
//			//cout << "\tLabel: " << lab_buf[i] << "\tFeas:";
//			// Now, separate the bunch into individual frames
//
//			// Asked by Ryan
//			// TODO: Why does Jeremy copy the original feature buffer to
//		    //       a new buffer instead of passing the original one? The
//		    //       new one will be deleted inside the Node class in
//		    //       CRF_StateNode::reset() and CRF_StateNode::~CRF_StateNode().
//
//			QNUInt32 cur_ftr_buf_size = num_ftrs * ftr_count;
//
//			float* new_buf = new float[cur_ftr_buf_size];
//
//			for (QNUInt32 j=0; j<cur_ftr_buf_size; j++) {
//				new_buf[j]=this->ftr_buf[j];
//				//cout << " " << new_buf[j];
//			}
//
//			//TODO: design a labmap class to do the label mapping.
//			QNUInt32 actualLab = CRF_LAB_BAD;
//			QNUInt32 ifBrokenLab = CRF_LAB_BAD;
//			QNUInt32 label = CRF_LAB_BAD;
//
////			for (QNUInt32 dur = 1; dur <= ftr_count; dur++)
////			{
////				actualLab = this->lab_buf[labs_width * (dur - 1)];
////
////				if (actualLab == CRF_LAB_BAD)
////					continue;
////				QNUInt32 begin = this->lab_buf[labs_width * (dur - 1) + 1];
////				QNUInt32 end = this->lab_buf[labs_width * (dur - 1) + 2];
////
////				assert(end - begin + 1 == dur);
////				//ifBrokenLab = this->lab_buf[labs_width * (dur - 1) + 3];
////				if (label != CRF_LAB_BAD)
////				{
////					string errstr="CRF_NewGradBuilder::buildGradient() caught exception: two valid labels found for the same frame.";
////					throw runtime_error(errstr);
////				}
////				label = nActualLabs * (dur - 1) + actualLab;
////				//label = nActualLabs * 2 * (dur - 1) + actualLab * 2 + ifBrokenLab;
////			}
//				actualLab = this->lab_buf[0];
//
//				if (actualLab != CRF_LAB_BAD)
//				{
//					QNUInt32 begin = this->lab_buf[1];
//					QNUInt32 end = this->lab_buf[2];
//
//					QNUInt32 dur = end - begin + 1;
//
//					ifBrokenLab = this->lab_buf[3];
////					label = nActualLabs * (dur - 1) + actualLab;
//					label = nActualLabs * (dur - 1) + actualLab * 2 + ifBrokenLab;
//				}
//
//			//cout << endl;
//			// Store the current frame/label information in a sequence node
//			//	* sequence nodes create a doubly-linked list, with the previous node known at creation time
//			//  * new_buf will be deleted when this sequence object gets deleted
//			/*if (nodeCnt >= this->nodeList->size() ) {
//				this->nodeList.push_back(new CRF_StdStateNodeLog(new_buf,num_ftrs,this->lab_buf[i],this->crf));
//			}
//			else {
//				this->nodeList.at(nodeCnt)->reset(new_buf,num_ftrs,this->lab_buf[i],this->crf);
//			}*/
//			QNUInt32 nodeMaxDur;
//			if (nodeCnt + 1 <= lab_max_dur)
//			{
//				nodeMaxDur = nodeCnt + 1;
//			}
//			else
//			{
//				nodeMaxDur = lab_max_dur;
//			}
//			// TODO: For segmental CRFs which have the different structures for different nodes, these parameters need to be changed.
//			QNUInt32 prevNode_nLabs = this->crf->getNLabs();
//			QNUInt32 nextNode_nActualLabs = nActualLabs;
//
//			this->nodeList->set(nodeCnt,new_buf,cur_ftr_buf_size,label,this->crf,nodeMaxDur,prevNode_nLabs,nextNode_nActualLabs);
//
//			QNUInt32 numPrevNodes;
//			if (nodeCnt + 1 <= lab_max_dur)
//			{
//				numPrevNodes = nodeCnt;
//			}
//			else
//			{
//				numPrevNodes = lab_max_dur;
//			}
//			assert(numPrevNodes + 1 == nodeMaxDur || numPrevNodes == nodeMaxDur);
//			CRF_StateNode** prevNodes = NULL;
//			if (numPrevNodes > 0)
//			{
//				prevNodes = new CRF_StateNode*[numPrevNodes];
//				for (QNUInt32 i = 0; i < numPrevNodes; i++)
//				{
//					QNUInt32 ni = nodeCnt - numPrevNodes + i;
//
//					prevNodes[i] = this->nodeList->at(ni);
//				}
//			}
//			this->nodeList->at(nodeCnt)->setPrevNodes(prevNodes, numPrevNodes);
//			this->nodeList->at(nodeCnt)->computeTransMatrix();
//			double scale;
//			if (nodeCnt == 0) {
//				scale=this->nodeList->at(nodeCnt)->computeFirstAlpha();
//			}
//			else {
//				scale=this->nodeList->at(nodeCnt)->computeAlpha();
//			}
//			//scale=this->nodeList->at(nodeCnt)->computeAlpha(prev_alpha);
//
//			//double sum = this->nodeList.at(nodeCnt)->computeAlphaSum();
//			//double* alpha=this->nodeList.at(nodeCnt)->getAlpha();
//
//			logLi-=scale;
//			nodeCnt++;
//			// End of Loop:
//			//	alpha[i] = alpha[i-1]*M[i]
//		}
//	} while (ftr_count > 0);

	// added by Ryan
	if (nodeCnt == 0)
	{
		string errstr="CRF_NewGradBuilder::buildGradient() caught exception: No features read from this sentence.";
		throw runtime_error(errstr);
	}

	nodeCnt--;//Correct for the fact that we add 1 to the nodeCnt at the end of the above loop...
	QNUInt32 lastNode=nodeCnt;

	double Zx=this->nodeList->at(lastNode)->computeAlphaSum();

	//double* tmpAlpha=this->nodeList->at(lastNode)->getAlpha();
	//int alpha_size=this->crf->getNLabs();
	//cout << "logLi currently: " << logLi << endl;
	//for (int tmp_i=0; tmp_i<alpha_size; tmp_i++) {
	//	cout << tmp_i << " :" << tmpAlpha[tmp_i] << endl;
	//}

	// Changed by Ryan
	bool stop=false;
	while (!stop) {
		double* beta = this->nodeList->at(nodeCnt)->getBeta();
		if (nodeCnt==lastNode) {
			this->nodeList->at(nodeCnt)->setTailBeta();
		}
		else {
			// We compute the beta value for the node following our current one, and store the result
			// as the beta for our current node (as per the equations).
			this->nodeList->at(nodeCnt+1)->computeBeta(beta,this->nodeList->at(nodeCnt)->getAlphaScale());
		}
		double* prev_alpha;
		QNUInt32 prev_lab;
		if (nodeCnt>0) {
			prev_alpha=this->nodeList->at(nodeCnt-1)->getAlpha();
			prev_lab = this->nodeList->at(nodeCnt-1)->getLabel();

		}
		else {
			prev_alpha=this->alpha_base;
			prev_lab=this->num_labs+1;
		}
		//double cur_alpha_sum = this->nodeList->at(nodeCnt)->computeAlphaSum(); //*DEBUG*//
		//cout << "\t" << nodeCnt << ":\tAlpha Sum: " << cur_alpha_sum;  //Added by Ryan, just for debugging
		logLi += this->nodeList->at(nodeCnt)->computeExpF(this->ExpF, grad, Zx, prev_alpha, prev_lab);
		//cout << "\t" << nodeCnt << ":\tLogLi is now: " << logLi << "\tAlpha Sum: " << cur_alpha_sum << endl; //*DEBUG*//
		//cout << "\tLogLi is now: " << logLi << endl;  //Added by Ryan, just for debugging
		if (nodeCnt==0) { stop=true;} // nodeCnt is unsigned, so we can't do the obvious loop control here
		nodeCnt--;
	}
//	bool stop=false;
//	while (!stop) {
//
//		QNUInt32 numNextNodes;
//		if (lastNode - nodeCnt <= lab_max_dur)
//		{
//			numNextNodes = lastNode - nodeCnt;
//		}
//		else
//		{
//			numNextNodes = lab_max_dur;
//		}
//		assert(numNextNodes <= lab_max_dur);
//		CRF_StateNode** nextNodes = NULL;
//		if (numNextNodes > 0)
//		{
//			nextNodes = new CRF_StateNode*[numNextNodes];
//			for (QNUInt32 i = 0; i < numNextNodes; i++)
//			{
//				QNUInt32 ni = nodeCnt + 1 + i;
//				nextNodes[i] = this->nodeList->at(ni);
//			}
//		}
//
//		this->nodeList->at(nodeCnt)->setNextNodes(nextNodes, numNextNodes);
//
//		if (nodeCnt==lastNode) {
//			this->nodeList->at(nodeCnt)->setTailBeta();
//		}
//		else {
//			this->nodeList->at(nodeCnt)->computeBeta(this->nodeList->at(nodeCnt)->getAlphaScale());
//		}
//
//		QNUInt32 prev_lab = CRF_LAB_BAD;
//		QNUInt32 prev_adj_seg_nodeCnt = nodeCnt;
//		while (prev_adj_seg_nodeCnt > 0) {
//			prev_lab = this->nodeList->at(prev_adj_seg_nodeCnt-1)->getLabel();
//			if (prev_lab != CRF_LAB_BAD)
//				break;
//			prev_adj_seg_nodeCnt--;
//		}
//
//		//double cur_alpha_sum = this->nodeList->at(nodeCnt)->computeAlphaSum(); //*DEBUG*//
//
//		logLi += this->nodeList->at(nodeCnt)->computeExpF(this->ExpF, grad, Zx, prev_lab);
//
//		if (this->nodeList->at(nodeCnt)->getPrevNodes() != NULL)
//			delete [] this->nodeList->at(nodeCnt)->getPrevNodes();
//
//		if (nextNodes != NULL)
//			delete [] nextNodes;
//
////		this->nodeList->at(nodeCnt)->deleteFtrBuf();
//
//		if (nodeCnt==0) { stop=true;} // nodeCnt is unsigned, so we can't do the obvious loop control here
//		nodeCnt--;
//	}

	for (QNUInt32 i=0; i<lambda_len; i++) {
		grad[i]-=this->ExpF[i];
	}
	*Zx_out=Zx;
	//logLi-=Zx;

	//nodeList.clear();
	return logLi;
}

