#include "../../include/interactive_mid_protocols/SigmaProtocolDH.hpp"

void SigmaDHMsg::initFromString(const string & s) {
	auto str_vec = explode(s, ':');
	assert(str_vec.size() == 2);
	a->initFromString(str_vec[0]);
	b->initFromString(str_vec[1]);
}

/********************************************/
/*       Sigma DH Simulator                 */
/********************************************/

/**
* Constructor that gets the underlying DlogGroup, soundness parameter and SecureRandom.
* @param dlog
* @param t Soundness parameter in BITS.
* @param random
*/
SigmaDHSimulator::SigmaDHSimulator(shared_ptr<DlogGroup> dlog, int t, mt19937 random) {
	//Sets the parameters.
	this->dlog = dlog;
	this->t = t;

	//Check the soundness validity.
	if (!checkSoundnessParam()) {
		throw invalid_argument("soundness parameter t does not satisfy 2^t<q");
	}

	this->random = random;
	qMinusOne = dlog->getOrder() - 1;
}

/**
* Computes the simulator computation.
* @param input MUST be an instance of SigmaDHCommonInput.
* @param challenge
* @return the output of the computation - (a, e, z).
* @throws CheatAttemptException if the received challenge's length is not equal to the soundness parameter.
* @throws IllegalArgumentException if the given input is not an instance of SigmaDHCommonInput.
*/
shared_ptr<SigmaSimulatorOutput> SigmaDHSimulator::simulate(SigmaCommonInput* input, vector<byte> challenge) {
	//check the challenge validity.
	if (!checkChallengeLength(challenge.size())) {
		throw CheatAttemptException("the length of the given challenge is differ from the soundness parameter");
	}

	SigmaDHCommonInput* dhInput = dynamic_cast<SigmaDHCommonInput*>(input);
	if (dhInput == NULL) {
		throw invalid_argument("the given input must be an instance of SigmaDHInput");
	}
	
	//Sample a random z <- Zq
	biginteger z = getRandomInRange(0, qMinusOne, random);

	//Compute a = g^z*u^(-e) (where -e here means -e mod q)
	auto gToZ = dlog->exponentiate(dlog->getGenerator().get(), z);
	biginteger e = decodeBigInteger(challenge.data(), challenge.size());
	biginteger minusE = dlog->getOrder() - e;
	auto uToE = dlog->exponentiate(dhInput->getU().get(), minusE);
	auto a = dlog->multiplyGroupElements(gToZ.get(), uToE.get());

	//Compute b = h^z*v^(-e) (where -e here means -e mod q)
	auto hToZ = dlog->exponentiate(dhInput->getH().get(), z);
	auto vToE = dlog->exponentiate(dhInput->getV().get(), minusE);
	auto b = dlog->multiplyGroupElements(hToZ.get(), vToE.get());

	//Output ((a,b),e,z).
	return make_shared<SigmaSimulatorOutput>(make_shared<SigmaDHMsg>(a->generateSendableData(), b->generateSendableData()), challenge, make_shared<SigmaBIMsg>(z));

}

/**
* Computes the simulator computation.
* @param input MUST be an instance of SigmaDHInput.
* @return the output of the computation - (a, e, z).
* @throws IllegalArgumentException if the given input is not an instance of SigmaDHInput.
*/
shared_ptr<SigmaSimulatorOutput> SigmaDHSimulator::simulate(SigmaCommonInput* input) {
	//Create a new byte array of size t/8, to get the required byte size and fill it with random values.
	vector<byte> e;
	gen_random_bytes_vector(e, t / 8, random);

	//Call the other simulate function with the given input and the sampled e.
	return simulate(input, e);
}

/**
* Checks the validity of the given soundness parameter.
* @return true if the soundness parameter is valid; false, otherwise.
*/
bool SigmaDHSimulator::checkSoundnessParam() {
	//If soundness parameter does not satisfy 2^t<q, return false.
	biginteger soundness = mp::pow(biginteger(2), t);
	biginteger q = dlog->getOrder();
	return (soundness < q);
}

/********************************************/
/*       Sigma DH Prover Computation        */
/********************************************/

/**
* Constructor that gets the underlying DlogGroup, soundness parameter and SecureRandom.
* @param dlog
* @param t Soundness parameter in BITS.
* @param random
* @throws IllegalArgumentException if soundness parameter is invalid.
*/
SigmaDHProverComputation::SigmaDHProverComputation(shared_ptr<DlogGroup> dlog, int t, mt19937 random) {

	//Sets the parameters.
	this->dlog = dlog;
	this->t = t;

	//Check the soundness validity.
	if (!checkSoundnessParam()) {
		throw invalid_argument("soundness parameter t does not satisfy 2^t<q");
	}

	this->random = random;
	qMinusOne = dlog->getOrder() - 1;
}

/**
* Computes the first message of the protocol.<p>
* "SAMPLE a random r in Zq<p>
* COMPUTE a = g^r and b = h^r".
* @param input MUST be an instance of SigmaDHProverInput.
* @return the computed message
* @throws IllegalArgumentException if input is not an instance of SigmaDHProverInput.
*/
shared_ptr<SigmaProtocolMsg> SigmaDHProverComputation::computeFirstMsg(shared_ptr<SigmaProverInput> input) {
	this->input = dynamic_pointer_cast<SigmaDHProverInput>(input);
	if (this->input == NULL) {
		throw invalid_argument("the given input must be an instance of SigmaDHProverInput");
	}
	
	//Sample random r in Zq
	r = getRandomInRange(0, qMinusOne, random);

	//Compute a = g^r.
	auto a = dlog->exponentiate(dlog->getGenerator().get(), r);
	//Compute b = h^r.
	shared_ptr<SigmaDHCommonInput> common = dynamic_pointer_cast<SigmaDHCommonInput>(input->getCommonInput());
	auto b = dlog->exponentiate(common->getH().get(), r);
	//Create and return SigmaDHMsg with a and b.
	return make_shared<SigmaDHMsg>(a->generateSendableData(), b->generateSendableData());
}

/**
* Computes the second message of the protocol.<p>
* "COMPUTE z = (r + ew) mod q".
* @param challenge
* @return the computed message.
* @throws CheatAttemptException if the length of the received challenge is not equal to the soundness parameter.
*/
shared_ptr<SigmaProtocolMsg> SigmaDHProverComputation::computeSecondMsg(vector<byte> challenge) {

	//check the challenge validity.
	if (!checkChallengeLength(challenge.size())) {
		throw new CheatAttemptException("the length of the given challenge is differ from the soundness parameter");
	}

	//Compute z = (r+ew) mod q
	biginteger q = dlog->getOrder();
	biginteger e = decodeBigInteger(challenge.data(), challenge.size());
	biginteger ew = (e * input->getW()) % q;
	biginteger z = (r + ew) % q;

	r = 0; //reset the random value r

	//Create and return SigmaBIMsg with z.
	return make_shared<SigmaBIMsg>(z);
}

/**
* Checks the validity of the given soundness parameter.
* @return true if the soundness parameter is valid; false, otherwise.
*/
bool SigmaDHProverComputation::checkSoundnessParam() {
	//If soundness parameter does not satisfy 2^t<q, return false.
	biginteger soundness = mp::pow(biginteger(2), t);
	biginteger q = dlog->getOrder();
	return (soundness < q);
}

/********************************************/
/*       Sigma DH Verifier Computation      */
/********************************************/

/**
* Constructor that gets the underlying DlogGroup, soundness parameter and SecureRandom.
* @param dlog
* @param t Soundness parameter in BITS.
* @param random
* @throws InvalidDlogGroupException if the given dlog is invalid.
* @throws IllegalArgumentException if soundness parameter is invalid.
*/
SigmaDHVerifierComputation::SigmaDHVerifierComputation(shared_ptr<DlogGroup> dlog, int t, mt19937 random) {

	if (!dlog->validateGroup())
		throw InvalidDlogGroupException("invalid dlog");

	//Sets the parameters.
	this->dlog = dlog;
	this->t = t;

	//Check the soundness validity.
	if (!checkSoundnessParam()) {
		throw invalid_argument("soundness parameter t does not satisfy 2^t<q");
	}

	this->random = random;

}

/**
* Checks the validity of the given soundness parameter.
* @return true if the soundness parameter is valid; false, otherwise.
*/
bool SigmaDHVerifierComputation::checkSoundnessParam() {
	//If soundness parameter does not satisfy 2^t<q, return false.
	biginteger soundness = mp::pow(biginteger(2), t);
	biginteger q = dlog->getOrder();
	return (soundness < q);
}

/**
* Samples the challenge of the protocol.<P>
* 	"SAMPLE a random challenge e<-{0,1}^t".
*/
void SigmaDHVerifierComputation::sampleChallenge() {
	//Create a new byte array of size t/8, to get the required byte size.
	gen_random_bytes_vector(e, t/8, random);
}

/**
* Computers the protocol's verification.<p>
* Computes the following line from the protocol:<p>
* 	"ACC IFF VALID_PARAMS(G,q,g) = TRUE AND h in G AND g^z = au^e  AND h^z = bv^e".   <p>
* @param input MUST be an instance of SigmaDHCommonInput.
* @param z second message from prover
* @return true if the proof has been verified; false, otherwise.
* @throws IllegalArgumentException if input is not an instance of SigmaDHCommonInput.
* @throws IllegalArgumentException if the first message of the prover is not an instance of SigmaDHMsg
* @throws IllegalArgumentException if the second message of the prover is not an instance of SigmaBIMsg
*/
bool SigmaDHVerifierComputation::verify(SigmaCommonInput* input, SigmaProtocolMsg* a, SigmaProtocolMsg* z) {
	SigmaDHCommonInput* dhInput = dynamic_cast<SigmaDHCommonInput*>(input);
	if (dhInput == NULL) {
		throw invalid_argument("the given input must be an instance of SigmaDHCommonInput");
	}

	bool verified = true;

	SigmaDHMsg* firstMsg = dynamic_cast<SigmaDHMsg*>(a);
	SigmaBIMsg* exponent = dynamic_cast<SigmaBIMsg*>(z);
	//If one of the messages is illegal, throw exception.
	if (firstMsg == NULL) {
		throw invalid_argument("first message must be an instance of SigmaDHMsg");
	}
	if (exponent == NULL) {
		throw invalid_argument("second message must be an instance of SigmaBIMsg");
	}

	//Get the h from the input and verify that it is in the Dlog Group.
	auto h = dhInput->getH();
	//If h is not member in the group, set verified to false.
	verified = verified && dlog->isMember(h.get());
	
	//Get the elements of the first message from the prover.
	auto aElement = dlog->reconstructElement(true, firstMsg->getA().get());
	auto bElement = dlog->reconstructElement(true, firstMsg->getB().get());

	//Verify that g^z = au^e:
	//Compute g^z (left size of the equation).
	auto left = dlog->exponentiate(dlog->getGenerator().get(), exponent->getMsg());
	//Compute a*u^e (right side of the verify equation).
	biginteger eBI = decodeBigInteger(e.data(), e.size()); 	// convert e to biginteger.
	
	//Calculate u^e.
	auto uToe = dlog->exponentiate(dhInput->getU().get(), eBI);
	//Calculate a*h^e.
	auto right = dlog->multiplyGroupElements(aElement.get(), uToe.get());
	//If left and right sides of the equation are not equal, set verified to false.
	verified = verified && (*left == *right);
	
	//Verify that h^z = bv^e:
	//Compute h^z (left size of the equation).
	left = dlog->exponentiate(h.get(), exponent->getMsg());
	//Compute b*v^e (right side of the verify equation).
	//Calculate v^e.
	auto vToe = dlog->exponentiate(dhInput->getV().get(), eBI);
	//Calculate b*v^e.
	right = dlog->multiplyGroupElements(bElement.get(), vToe.get());
	//If left and right sides of the equation are not equal, set verified to false.
	verified = verified && (*left == *right);
	
	e.clear(); //Delete the random value e for re-use.

	//Return true if all checks returned true; false, otherwise.
	return verified;
}