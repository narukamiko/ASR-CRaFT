#include "CRF_NewGradBuilderSparseLog.h"

CRF_NewGradBuilderSparseLog::CRF_NewGradBuilderSparseLog(CRF_Model* crf_in)
	: CRF_NewGradBuilder(crf_in)
{
	for (QNUInt32 i=0; i<this->num_labs; i++) {
		this->alpha_base[i]=0.0;
	}
	this->nodeList = new CRF_StdStateVectorLog();
}

CRF_NewGradBuilderSparseLog::~CRF_NewGradBuilderSparseLog()
{
}

double CRF_NewGradBuilderSparseLog::buildGradient(CRF_FeatureStream* ftr_strm, double* grad, double* Zx_out)
{
	QNUInt32 lambda_len = this->crf->getLambdaLen();

	double logLi = 0.0;
	size_t bunch_size = 3;
	size_t num_ftrs=ftr_strm->num_ftrs();  // Count coming off feature stream will be doubled for index/value pairing
	if (this->ftr_buf==NULL) {  // First pass through initialize the buffers
		this->ftr_buf = new float[num_ftrs*bunch_size];
		this->lab_buf = new QNUInt32[bunch_size];
	}

	size_t ftr_count;

	for (QNUInt32 i=0; i<lambda_len; i++) {
		this->ExpF[i]=0.0;
	}

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

	nodeCnt--;//Correct for the fact that we add 1 to the nodeCnt at the end of the above loop...
	QNUInt32 lastNode=nodeCnt;
	double Zx=this->nodeList->at(lastNode)->computeAlphaSum();

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
		logLi += this->nodeList->at(nodeCnt)->computeExpF(this->ExpF, grad, Zx, prev_alpha, prev_lab);
		if (nodeCnt==0) { stop=true;} // nodeCnt is unsigned, so we can't do the obvious loop control here
		nodeCnt--;
	}

	for (QNUInt32 i=0; i<lambda_len; i++) {
		grad[i]-=this->ExpF[i];
	}
	logLi-=Zx;

	*Zx_out=Zx;
	//nodeList.clear();
	return logLi;
}