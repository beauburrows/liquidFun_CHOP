/* Shared Use License: This file is owned by Derivative Inc. (Derivative) and
 * can only be used, and/or modified for use, in conjunction with 
 * Derivative's TouchDesigner software, and only if you are a licensee who has
 * accepted Derivative's TouchDesigner license or assignment agreement (which
 * also govern the use of this file).  You may share a modified version of this
 * file with another authorized licensee of Derivative's TouchDesigner software.
 * Otherwise, no redistribution or sharing of this file, with or without
 * modification, is permitted.
 */

#include "CPlusPlusCHOPExample.h"

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <assert.h>

#include <Box2D/Box2D.h>
#include <stdlib.h>

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

	DLLEXPORT
		int32_t
		GetCHOPAPIVersion(void)
	{
		// Always return CHOP_CPLUSPLUS_API_VERSION in this function.
		return CHOP_CPLUSPLUS_API_VERSION;
	}

	DLLEXPORT
		CHOP_CPlusPlusBase*
		CreateCHOPInstance(const OP_NodeInfo* info)
	{
		// Return a new instance of your class every time this is called.
		// It will be called once per CHOP that is using the .dll
		return new CPlusPlusCHOPExample(info);
	}

	DLLEXPORT
		void
		DestroyCHOPInstance(CHOP_CPlusPlusBase* instance)
	{
		// Delete the instance here, this will be called when
		// Touch is shutting down, when the CHOP using that instance is deleted, or
		// if the CHOP loads a different DLL
		delete (CPlusPlusCHOPExample*)instance;
	}

};

CPlusPlusCHOPExample::CPlusPlusCHOPExample(const OP_NodeInfo* info) : myNodeInfo(info)
{
	myExecuteCount = 0;
	myOffset = 0.0;

	reset = 0;
	spawn = 0;
	pCount = 0;

	wallLeftPos.Set(-50.0f, 0.0f);
	wallRightPos.Set(50.0f, 0.0f);
	wallTopPos.Set(0.0f, 50.0f);

	wallLeftSize.Set(10.0f, 200.0f);
	wallRightSize.Set(10.0f, 200.0f);
	wallTopSize.Set(200.0f, 10.0f);

	forceParticles = b2Vec2(0.0f, 0.0f);
	forceBodies = b2Vec2(0.0f, 0.0f);

	particleType = 0;
	particleSize = 1.0f;

	inNumParts = 0;
}

CPlusPlusCHOPExample::~CPlusPlusCHOPExample()
{

}

void
CPlusPlusCHOPExample::getGeneralInfo(CHOP_GeneralInfo* ginfo)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = true;
	// This will define if the output is time sliced
	ginfo->timeslice = false;

	ginfo->inputMatchIndex = 0;
}

bool
CPlusPlusCHOPExample::getOutputInfo(CHOP_OutputInfo* info)
{
	// Specify our own output info
	info->numChannels = 7;

	// Since we are not outputting a timeslice, the system will not dictate
	// the numSamples and startIndex of the CHOP data, so we have to specify
	info->numSamples = pCount; // this might need to change <<<<<<<<<<<<<<
	info->startIndex = 0;

	// We are going to output 60hz data (since we are assuming the project is 
	// running at 60hz)
	info->sampleRate = 60;

	return true;
}

const char*
CPlusPlusCHOPExample::getChannelName(int32_t index, void* reserved)
{
	return "chan1";
}

///////////////////////////////////////////////////////////////////////////
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// particle stuff initialization

// Define the gravity vector of 0 (no gravity to start)
b2Vec2 gravity(0.0f, 0.0f);
// Construct a world object, which will hold and simulate the rigid bodies.
b2World world(gravity);


// Define the ground body.
b2BodyDef groundBodyDef;

// Call the body factory which allocates memory for the ground body
// from a pool and creates the ground box shape (also from a pool).
// The body is also added to the world.
b2Body* groundBody;

// Define the ground box shape.
b2PolygonShape groundBox;

void
CPlusPlusCHOPExample::m_generateGroundPlane()
{
	groundBodyDef.position.Set(0.0f, -10.0f);
	groundBody = world.CreateBody(&groundBodyDef);

	// The extents are the half-widths of the box.
	groundBox.SetAsBox(200.0f, 10.0f);

	// Add the ground fixture to the ground body.
	groundBody->CreateFixture(&groundBox, 0.0f);
}

void
CPlusPlusCHOPExample::m_generateStaticBox(b2Vec2 pos, b2Vec2 size)
{
	b2BodyDef bodyDef;
	b2Body* body;
	// Define another box shape for our dynamic body.
	b2PolygonShape staticBox;

	bodyDef.type = b2_staticBody;
	bodyDef.position.Set(pos.x, pos.y);
	body = world.CreateBody(&bodyDef);
	// The extents are the half-widths of the box.
	staticBox.SetAsBox(size.x, size.y);
	// Add the box fixture to the box body.
	body->CreateFixture(&staticBox, 0.0f);
}

void
CPlusPlusCHOPExample::m_generateDynamicCircle(int* eID, b2Vec2 pos, b2Vec2 vel, float size)
{
	// Define the dynamic body. We set its position and call the body factory.
	b2BodyDef bodyDef;
	bodyDef.type = b2_dynamicBody;
	bodyDef.position.Set(pos.x, pos.y);
	bodyDef.linearVelocity.Set(vel.x, vel.y);
	b2Body* body = world.CreateBody(&bodyDef);

	// Define a circle shape for our dynamic body.
	b2CircleShape dynamicCircle;
	dynamicCircle.m_radius = size;

	// Define the dynamic body fixture.
	b2FixtureDef fixtureDef;
	fixtureDef.shape = &dynamicCircle;

	// Set the box density to be non-zero, so it will be dynamic.
	fixtureDef.density = 1.0f;

	// Override the default friction <<< UPDATE???
	// fixtureDef.friction = 0.3f;

	body->SetUserData(eID);

	// Add the shape to the body.
	body->CreateFixture(&fixtureDef);
}

// declare particle system
const b2ParticleSystemDef particleSystemDef;
b2ParticleSystem* m_particleSystem;

// declare point count tracker
int32 m_pointCount;

void
CPlusPlusCHOPExample::m_spawnParticle(int* eID, b2Vec2 pos, b2Vec2 vel)
{
	b2ParticleDef pd;
	pd.flags = b2_elasticParticle;
	pd.color.Set(0, 0, 255, 255);
	pd.position.Set(pos.x, pos.y);
	pd.velocity.Set(vel.x, vel.y);
	pd.userData = eID;
	int tempIndex = m_particleSystem->CreateParticle(pd);

	++m_pointCount;
}

// Prepare for simulation. Typically we use a time step of 1/60 of a
// second (60Hz) and 10 iterations. This provides a high quality simulation
// in most game scenarios.
float32 timeStep = 1.0f / 60.0f;
int32 velocityIterations = 6;
int32 positionIterations = 2;
int32 particleIterations = 2;

// end particle stuff initialization
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
///////////////////////////////////////////////////////////////////////////

void
CPlusPlusCHOPExample::execute(const CHOP_Output* output,
	OP_Inputs* inputs,
	void* reserved)
{
	myExecuteCount++;

	double body1x = inputs->getParDouble("Walls", 0);
	double body1y = inputs->getParDouble("Walls", 1);
	double body2x = inputs->getParDouble("Walls", 2);
	double body2y = inputs->getParDouble("Walls", 3);

	if (myExecuteCount > 2) {
		b2Body* tempBody = world.GetBodyList();
		tempBody = tempBody->GetNext();
		tempBody->SetTransform(b2Vec2(body1x, body1y), 0.0);
		tempBody->GetNext()->SetTransform(b2Vec2(body2x, body2y), 0.0);
	}
	else if (myExecuteCount < 2) {

		// REMAKE WORLD
		// generate ground plane
		m_generateGroundPlane();

		m_generateStaticBox(wallLeftPos, wallLeftSize);
		m_generateStaticBox(wallRightPos, wallRightSize);
		m_generateStaticBox(wallTopPos, wallTopSize);

		m_pointCount = 0;

		m_particleSystem = world.CreateParticleSystem(&particleSystemDef);

		// spawn initial particles
		
		for (int i = 0; i < 10; i++) {
			int* x = &i;
			b2ParticleDef pd;
			pd.flags = b2_elasticParticle;
			pd.color.Set(0, 0, 255, 255);
			pd.position.Set((i%10)*2 - 10 + float(i)/1000, int(i/10)*2 + 5);
			pd.userData = x;
			int tempIndex = m_particleSystem->CreateParticle(pd);

			++m_pointCount;
		}
		

		/*
		// Define the gravity vector.
		b2Vec2 gravity(0.0f, -10.0f);

		// Construct a world object, which will hold and simulate the rigid bodies.
		b2World world(gravity);
		
		groundBodyDef.position.Set(0.0f, -10.0f);

		// Call the body factory which allocates memory for the ground body
		// from a pool and creates the ground box shape (also from a pool).
		// The body is also added to the world.
		b2Body* groundBody = world.CreateBody(&groundBodyDef);



		// The extents are the half-widths of the box.
		groundBox.SetAsBox(50.0f, 10.0f);

		// Add the ground fixture to the ground body.
		groundBody->CreateFixture(&groundBox, 0.0f);


		bodyDef.type = b2_dynamicBody;
		bodyDef.position.Set(0.0f, 4.0f);
		body = world.CreateBody(&bodyDef);

		// Define another box shape for our dynamic body.
		b2PolygonShape dynamicBox;
		dynamicBox.SetAsBox(1.0f, 1.0f);

		// Define the dynamic body fixture.
		b2FixtureDef fixtureDef;
		fixtureDef.shape = &dynamicBox;

		// Set the box density to be non-zero, so it will be dynamic.
		fixtureDef.density = 1.0f;

		// Override the default friction.
		fixtureDef.friction = 0.3f;

		// Add the shape to the body.
		body->CreateFixture(&fixtureDef);
		*/


	}

	if (reset == 1)
	{
		// DELETE WORLD ...
		delete &world;

		// NEED TO REMAKE WORLDDDDDDDDDD

		// REMAKE WORLD
		// generate ground plane
		m_generateGroundPlane();

		m_generateStaticBox(wallLeftPos, wallLeftSize);
		m_generateStaticBox(wallRightPos, wallRightSize);
		m_generateStaticBox(wallTopPos, wallTopSize);

		pCount = 0;
		spawn = 0;
		m_pointCount = 0;

		m_particleSystem = world.CreateParticleSystem(&particleSystemDef);

		reset = 0;
	}

	// Now print the position and angle of the body.
	//b2Vec2 position = body->GetPosition();
	//float32 angle = body->GetAngle();

	// If the input is present, read info and spawn/alter system accordingly
	if (inputs->getNumInputs() > 0 && spawn == 1)
	{
		/*
		int ind = 0;
		for (int i = 0 ; i < output->numChannels; i++)
		{
			for (int j = 0; j < output->numSamples; j++)
			{
				const OP_CHOPInput	*cinput = inputs->getInputCHOP(0);
				 = float(cinput->getChannelData(i)[ind]);
				ind++;

				// Make sure we don't read past the end of the CHOP input
				ind = ind % cinput->numSamples;
			}
		}
		*/

		const OP_CHOPInput	*cinput = inputs->getInputCHOP(0);

		for (int j = 0; j < cinput->numSamples; j++)
		{
			int eID = int(cinput->getChannelData(0)[j]);
			float x = float(cinput->getChannelData(1)[j]);
			float y = float(cinput->getChannelData(2)[j]);
			float vX = float(cinput->getChannelData(3)[j]);
			float vY = float(cinput->getChannelData(4)[j]);
			float size = float(cinput->getChannelData(5)[j]);

			// change 1.0 to default size
			if (size == float(1.0))
			{
				// spawn as particle with default size
				m_spawnParticle(&eID, b2Vec2(x, y), b2Vec2(vX, vY));
			}
			else
			{
				// spawn as body with custom size
				m_generateDynamicCircle(&eID, b2Vec2(x, y), b2Vec2(vX, vY), size);
			}
		}

		spawn = 0;

	}

	// Instruct the world to perform a single step of simulation.
	// It is generally best to keep the time step and iterations fixed.
	world.Step(timeStep, velocityIterations, positionIterations, particleIterations);
	world.ClearForces();
	
	pCount = m_particleSystem->GetParticleCount();

	// output positions
	if (pCount > 0)
	{
		//for (int j = 0; j < output->numSamples; j++)
		for (int j = 0; j < pCount; j++)
		{
			output->channels[0][j] = int(m_particleSystem->GetUserDataBuffer()[j]);
			output->channels[1][j] = float(m_particleSystem->GetPositionBuffer()[j].x);
			output->channels[2][j] = float(m_particleSystem->GetPositionBuffer()[j].y);
			output->channels[3][j] = float(m_particleSystem->GetVelocityBuffer()[j].x);
			output->channels[4][j] = float(m_particleSystem->GetVelocityBuffer()[j].y);
			output->channels[5][j] = float(m_particleSystem->GetRadius());
			output->channels[6][j] = pCount;
		}
	}

	/*
	else // If no input is connected, use default system values
	{
		inputs->enablePar("Walls", 1);

		
		for (int j = 0; j < output->numSamples; j++)
		{
			output->channels[0][j] = float(m_particleSystem->GetPositionBuffer()[j].x);
			output->channels[1][j] = float(m_particleSystem->GetPositionBuffer()[j].y);
			output->channels[2][j] = float(m_particleSystem->GetVelocityBuffer()[j].x);
			output->channels[3][j] = float(m_particleSystem->GetVelocityBuffer()[j].y);
		}
		
	}
	*/
}

int32_t
CPlusPlusCHOPExample::getNumInfoCHOPChans()
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send one channel.
	return 2;
}

void
CPlusPlusCHOPExample::getInfoCHOPChan(int32_t index,
										OP_InfoCHOPChan* chan)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name = "executeCount";
		chan->value = (float)myExecuteCount;
	}

	if (index == 1)
	{
		chan->name = "offset";
		chan->value = (float)myOffset;
	}
}

bool		
CPlusPlusCHOPExample::getInfoDATSize(OP_InfoDATSize* infoSize)
{
	infoSize->rows = 2;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
CPlusPlusCHOPExample::getInfoDATEntries(int32_t index,
										int32_t nEntries,
										OP_InfoDATEntries* entries)
{
	// It's safe to use static buffers here because Touch will make it's own
	// copies of the strings immediately after this call returns
	// (so the buffers can be reuse for each column/row)
	static char tempBuffer1[4096];
	static char tempBuffer2[4096];

	if (index == 0)
	{
		// Set the value for the first column
#ifdef WIN32
		strcpy_s(tempBuffer1, "executeCount");
#else // macOS
        strlcpy(tempBuffer1, "executeCount", sizeof(tempBuffer1));
#endif
		entries->values[0] = tempBuffer1;

		// Set the value for the second column
#ifdef WIN32
		sprintf_s(tempBuffer2, "%d", myExecuteCount);
#else // macOS
        snprintf(tempBuffer2, sizeof(tempBuffer2), "%d", myExecuteCount);
#endif
		entries->values[1] = tempBuffer2;
	}

	if (index == 1)
	{
		// Set the value for the first column
#ifdef WIN32
        strcpy_s(tempBuffer1, "offset");
#else // macOS
        strlcpy(tempBuffer1, "offset", sizeof(tempBuffer1));
#endif
		entries->values[0] = tempBuffer1;

		// Set the value for the second column
#ifdef WIN32
        sprintf_s(tempBuffer2, "%g", myOffset);
#else // macOS
        snprintf(tempBuffer2, sizeof(tempBuffer2), "%g", myOffset);
#endif
		entries->values[1] = tempBuffer2;
	}
}

void
CPlusPlusCHOPExample::setupParameters(OP_ParameterManager* manager)
{
	// wall positions
	{
		OP_NumericParameter	np;

		np.name = "Walls";
		np.label = "Walls";
		np.defaultValues[0] = -20;
		np.defaultValues[1] = 0.0;
		np.defaultValues[2] = 20;
		np.defaultValues[3] = 0.0;

		OP_ParAppendResult res = manager->appendFloat(np, 4);
		assert(res == OP_ParAppendResult::Success);
	}

	/*
	// speed
	{
		OP_NumericParameter	np;

		np.name = "Speed";
		np.label = "Speed";
		np.defaultValues[0] = 1.0;
		np.minSliders[0] = -10.0;
		np.maxSliders[0] =  10.0;
		
		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// scale
	{
		OP_NumericParameter	np;

		np.name = "Scale";
		np.label = "Scale";
		np.defaultValues[0] = 1.0;
		np.minSliders[0] = -10.0;
		np.maxSliders[0] =  10.0;
		
		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// shape
	{
		OP_StringParameter	sp;

		sp.name = "Shape";
		sp.label = "Shape";

		sp.defaultValue = "Sine";

		const char *names[] = { "Sine", "Square", "Ramp" };
		const char *labels[] = { "Sine", "Square", "Ramp" };

		OP_ParAppendResult res = manager->appendMenu(sp, 3, names, labels);
		assert(res == OP_ParAppendResult::Success);
	}
	*/

	// pulse spawn
	{
		OP_NumericParameter	np;

		np.name = "Spawn";
		np.label = "Spawn";

		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}
	
	// pulse reset
	{
		OP_NumericParameter	np;

		np.name = "Reset";
		np.label = "Reset";
		
		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}
	

}

void 
CPlusPlusCHOPExample::pulsePressed(const char* name)
{
	
	if (!strcmp(name, "Reset"))
	{
		reset = 1;
	}

	if (!strcmp(name, "Spawn"))
	{
		spawn = 1;
	}
	
}

