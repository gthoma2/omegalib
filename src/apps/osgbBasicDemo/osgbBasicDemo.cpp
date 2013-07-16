#include <osgUtil/Optimizer>
#include <osg/PositionAttitudeTransform>
#include <osg/MatrixTransform>
#include <osg/Light>
#include <osg/LightSource>
#include <osg/Material>

#define OMEGA_NO_GL_HEADERS
#include <omega.h>
#include <omegaToolkit.h>
#include <omegaOsg.h>

#include <osgViewer/Viewer>
#include <osgDB/ReadFile>
#include <osgGA/TrackballManipulator>
#include <osgGA/GUIEventHandler>
#include <osg/MatrixTransform>
#include <osg/Geode>
#include <osgwTools/Shapes.h>
#include <osgwTools/Version.h>
#include <osg/ShapeDrawable>

#include <osgbDynamics/MotionState.h>
#include <osgbCollision/CollisionShapes.h>
#include <osgbDynamics/RigidBody.h>
#include <osgbCollision/Utils.h>

#include <btBulletDynamicsCommon.h>

#include <string>

#include <osg/io_utils>
#include <iostream>

//#include "GLDebugDrawer.h"

using namespace omega;
using namespace omegaToolkit;
using namespace omegaOsg;

//static GLDebugDrawer gDebugDraw;

///create 125 (5x5x5) dynamic object
#define ARRAY_SIZE_X 5
#define ARRAY_SIZE_Y 5
#define ARRAY_SIZE_Z 5

//maximum number of objects (and allow user to shoot additional boxes)
#define MAX_PROXIES (ARRAY_SIZE_X*ARRAY_SIZE_Y*ARRAY_SIZE_Z + 1024)

///scaling of the objects (0.1 = 20 centimeter boxes )
#define SCALING 1.
#define START_POS_X -5
#define START_POS_Y -5
#define START_POS_Z -3

// create a box
// 100% osg stuff
osg::Geode* osgBox( const osg::Vec3& center, const osg::Vec3& halfLengths )
{
    osg::Vec3 l( halfLengths * 2. );
    osg::Box* box = new osg::Box( center, l.x(), l.y(), l.z() );
    osg::ShapeDrawable* shape = new osg::ShapeDrawable( box );
    shape->setColor( osg::Vec4( 1., 0., 1., 1. ) );
    osg::Geode* geode = new osg::Geode();
    geode->addDrawable( shape );

    return( geode );
}

//================================================================================

class OsgbBasicDemo : public EngineModule
{
public:
	OsgbBasicDemo()
	{
		myOsg = new OsgModule();
		ModuleServices::addModule(myOsg);
		myWorld = initBtPhysicsWorld();
	}

	virtual void initialize();
	virtual void update(const UpdateContext& context);

	btDynamicsWorld* initBtPhysicsWorld();
	//void exitPhysics();

	//virtual void clientMoveAndDisplay();

	//virtual void displayCallback();
	//void resetScene();

private:

	Ref<OsgModule> myOsg;
	btDynamicsWorld* myWorld;
	Ref<SceneNode> mySceneNode;
	Actor* myInteractor;
	OsgSceneObject* myOsgSceneObj;

	osg::MatrixTransform* myGround;

	osg::MatrixTransform* myCol;

	//osgbDynamics::MotionState* myGroundMotionState;

	//osgbDynamics::MotionState* myColMotionState;
	//keep the collision shapes, for deletion/cleanup
	//btAlignedObjectArray<btCollisionShape*>	myCollisionShapes;
	
};

// initial steps to create bullet dynamics world
// 100% bullet stuff
btDynamicsWorld* OsgbBasicDemo::initBtPhysicsWorld()
{
	btDefaultCollisionConfiguration * collisionConfiguration = new btDefaultCollisionConfiguration();
	btCollisionDispatcher * dispatcher = new btCollisionDispatcher( collisionConfiguration );
	btBroadphaseInterface * inter = new btDbvtBroadphase();
	btConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

	btDynamicsWorld * dw = new btDiscreteDynamicsWorld( dispatcher, inter, solver, collisionConfiguration );
	
	//dw->setDebugDrawer(&gDebugDraw);
	dw->setGravity( btVector3(0, -10, 0) );
 
	return ( dw );
}

void OsgbBasicDemo::initialize()
{
	osg::Group* root = new osg::Group;

	//create a few basic rigid bodies
	osg::Vec3 halfLengths( 50., 50., 50. );
	osg::Vec3 center( 0., -50., 0 );
	btBoxShape* groundShape = new btBoxShape( osgbCollision::asBtVector3( halfLengths ) );
	//groundShape->initializePolyhedralFeatures();
//	btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0,1,0),50);
	
	//myCollisionShapes.push_back(groundShape);

	btTransform groundTransform;
	groundTransform.setIdentity();
	groundTransform.setOrigin( osgbCollision::asBtVector3( center ) );
	
	{
		myGround = new osg::MatrixTransform( osgbCollision::asOsgMatrix( groundTransform ) );
		myGround->addChild( osgBox( osg::Vec3(0,0,0), halfLengths ) );

		osgbDynamics::MotionState* ms = new osgbDynamics::MotionState();
		ms->setWorldTransform(groundTransform);
		ms->setTransform(myGround);
		//myGroundMotionState->setWorldTransform(groundTransform);
		//myGroundMotionState->setTransform(myGround);
		btScalar mass( 0.0 );
		btVector3 inertia( 0, 0, 0 );
		//btRigidBody::btRigidBodyConstructionInfo rb( mass, myGroundMotionState, groundShape, inertia );
		btRigidBody::btRigidBodyConstructionInfo rb( mass, ms, groundShape, inertia );
		btRigidBody* body = new btRigidBody( rb );

		myWorld->addRigidBody(body);
	}

	root->addChild(myGround);
	
	{
		//create a few dynamic rigidbodies
		// Re-using the same collision is better for memory usage and performance

		osg::Vec3 halfLengths(SCALING*1, SCALING*1,SCALING*1);

		btBoxShape* colShape = new btBoxShape( osgbCollision::asBtVector3( halfLengths ) );
		//btCollisionShape* colShape = new btSphereShape(btScalar(1.));
		//myCollisionShapes.push_back(colShape);

		/// Create Dynamic Objects
		btTransform startTransform;

		btScalar	mass(1.f);

		//rigidbody is dynamic if and only if mass is non zero, otherwise static
		bool isDynamic = (mass != 0.f);

		btVector3 localInertia(0,0,0);
		if (isDynamic)
			colShape->calculateLocalInertia(mass,localInertia);

		float start_x = START_POS_X - ARRAY_SIZE_X/2.0;
		float start_y = START_POS_Y;
		float start_z = START_POS_Z - ARRAY_SIZE_Z/2.0;

		mySceneNode = new SceneNode(getEngine());

		for (int k=0;k<ARRAY_SIZE_Y;k++)
		{
			for (int i=0;i<ARRAY_SIZE_X;i++)
			{
				for(int j = 0;j<ARRAY_SIZE_Z;j++)
				{
					// first one at (-7.5, 15, -5.5)
					osg::Vec3 center( btScalar(2.0*i + start_x), btScalar(20+2.0*k + start_y), btScalar(2.0*j + start_z) );

					startTransform.setIdentity();
					startTransform.setOrigin( SCALING * osgbCollision::asBtVector3(center) );

					myCol = new osg::MatrixTransform( osgbCollision::asOsgMatrix(startTransform) );
					myCol->addChild( osgBox( osg::Vec3(0,0,0), halfLengths ) );

					myOsgSceneObj = new OsgSceneObject(myCol);
					root->addChild( myOsgSceneObj->getTransformedNode() );

					mySceneNode->addComponent(myOsgSceneObj);

					//using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
					/*
					if (k==0 && i == 0 && j == 0) {
						myColMotionState->setWorldTransform(startTransform);
						myColMotionState->setTransform(myCol);
						btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myColMotionState, colShape, localInertia);
						btRigidBody* body = new btRigidBody(rbInfo);
						myWorld->addRigidBody(body);
					}*/
					//else {
						osgbDynamics::MotionState* motionState = new osgbDynamics::MotionState();
						motionState->setWorldTransform(startTransform);
						motionState->setTransform(myCol);
						btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, colShape, localInertia);
						btRigidBody* body = new btRigidBody(rbInfo);
						myWorld->addRigidBody(body);
					//}

				}
			}
		}
	}
    
    mySceneNode->setBoundingBoxVisible(true);
    //mySceneNode->setBoundingBoxVisible(false);
    getEngine()->getScene()->addChild(mySceneNode);
	getEngine()->getDefaultCamera()->setPosition(0,0,60);
	//getEngine()->getDefaultCamera()->setOrientation(1, 0, -1, -3);

    // Set the interactor style used to manipulate meshes.
    if(SystemManager::settingExists("config/interactor"))
    {
        Setting& sinteractor = SystemManager::settingLookup("config/interactor");
        myInteractor = ToolkitUtils::createInteractor(sinteractor);
        if(myInteractor != NULL)
        {
            ModuleServices::addModule(myInteractor);
        }
    }

    if(myInteractor != NULL)
    {
        myInteractor->setSceneNode(mySceneNode);
    }

    // Set the osg node as the root node
    myOsg->setRootNode(root);

	/*/
	printf("bullet:\n");
	for (int i=0;i<10;i++)
	{
		printf("object %d: (worldtrans) (%lf, %lf, %lf)\n", i,
			myWorld->getCollisionObjectArray().at(i)->getWorldTransform().getOrigin().x(),
			myWorld->getCollisionObjectArray().at(i)->getWorldTransform().getOrigin().y(),
			myWorld->getCollisionObjectArray().at(i)->getWorldTransform().getOrigin().z());
	}
	printf("\n");

	printf("osg:\n");
	for (int i=0;i<10;i++)
	{
		printf("object %d: (myOsg.child(i)) (%lf, %lf, %lf)\n", i,
			myOsg->getRootNode()->asGroup()->getChild(i)->asTransform()->asMatrixTransform()->getMatrix().getTrans().x(),
			myOsg->getRootNode()->asGroup()->getChild(i)->asTransform()->asMatrixTransform()->getMatrix().getTrans().y(),
			myOsg->getRootNode()->asGroup()->getChild(i)->asTransform()->asMatrixTransform()->getMatrix().getTrans().z());
	}
	//*/
}

void OsgbBasicDemo::update(const UpdateContext& context)
{
	double elapsed = 1./60.;
	myWorld->stepSimulation( elapsed, 4, elapsed/4. );

	//
	printf("bullet:\n");
	for (int i=0;i<5;i++)
	{
		printf("object %d: (worldtrans) (%lf, %lf, %lf)\n", i,
			myWorld->getCollisionObjectArray().at(i)->getWorldTransform().getOrigin().x(),
			myWorld->getCollisionObjectArray().at(i)->getWorldTransform().getOrigin().y(),
			myWorld->getCollisionObjectArray().at(i)->getWorldTransform().getOrigin().z());
	}
	//*/
	/*/
	printf("motion:\n");
	btTransform world;
	for (int i=0;i<5;i++)
	{
		ms[i]->getWorldTransform( world );
		printf("object %d: (worldtrans): (%lf, %lf, %lf)\n", i,
			world.getOrigin().x(), world.getOrigin().y(), world.getOrigin().z());
	}
	//*/
	//
	printf("osg:\n");
	for (int i=0;i<5;i++)
	{
		printf("object %d: (myOsg.child(i)) (%lf, %lf, %lf)\n", i,
			myOsg->getRootNode()->asGroup()->getChild(i)->asTransform()->asMatrixTransform()->getMatrix().getTrans().x(),
			myOsg->getRootNode()->asGroup()->getChild(i)->asTransform()->asMatrixTransform()->getMatrix().getTrans().y(),
			myOsg->getRootNode()->asGroup()->getChild(i)->asTransform()->asMatrixTransform()->getMatrix().getTrans().z());
	}
	//*/
}

int main(int argc, char** argv)
{
	Application<OsgbBasicDemo> app("osgbBasicDemo");
	return omain(app, argc, argv);
}