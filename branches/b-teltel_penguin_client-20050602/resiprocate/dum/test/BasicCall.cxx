#include "resiprocate/SdpContents.hxx"
#include "resiprocate/SipMessage.hxx"
#include "resiprocate/ShutdownMessage.hxx"
#include "resiprocate/SipStack.hxx"
#include "resiprocate/dum/ClientAuthManager.hxx"
#include "resiprocate/dum/ClientInviteSession.hxx"
#include "resiprocate/dum/ClientRegistration.hxx"
#include "resiprocate/dum/DialogUsageManager.hxx"
#include "resiprocate/dum/DumShutdownHandler.hxx"
#include "resiprocate/dum/InviteSessionHandler.hxx"
#include "resiprocate/dum/MasterProfile.hxx"
#include "resiprocate/dum/RegistrationHandler.hxx"
#include "resiprocate/dum/ServerInviteSession.hxx"
#include "resiprocate/dum/ServerOutOfDialogReq.hxx"
#include "resiprocate/dum/OutOfDialogHandler.hxx"
#include "resiprocate/dum/AppDialog.hxx"
#include "resiprocate/dum/AppDialogSet.hxx"
#include "resiprocate/dum/AppDialogSetFactory.hxx"
#include "resiprocate/os/Log.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/os/Random.hxx"

#include <sstream>
#include <time.h>

#define RESIPROCATE_SUBSYSTEM Subsystem::TEST

using namespace resip;
using namespace std;

void sleepSeconds(unsigned int seconds)
{
#ifdef WIN32
   Sleep(seconds*1000);
#else
   sleep(seconds);
#endif
}

/////////////////////////////////////////////////////////////////////////////////
//
// Classes that provide the mapping between Application Data and DUM 
// dialogs/dialogsets
//  										
// The DUM layer creates an AppDialog/AppDialogSet object for inbound/outbound
// SIP Request that results in Dialog creation.
//  										
/////////////////////////////////////////////////////////////////////////////////
class testAppDialog : public AppDialog
{
public:
   testAppDialog(HandleManager& ham, Data &SampleAppData) : AppDialog(ham), mSampleAppData(SampleAppData)
   {  
      cout << mSampleAppData << ": testAppDialog: created." << endl;  
   }
   virtual ~testAppDialog() 
   { 
      cout << mSampleAppData << ": testAppDialog: destroyed." << endl; 
   }
   Data mSampleAppData;
};

class testAppDialogSet : public AppDialogSet
{
public:
   testAppDialogSet(DialogUsageManager& dum, Data SampleAppData) : AppDialogSet(dum), mSampleAppData(SampleAppData)
   {  
      cout << mSampleAppData << ": testAppDialogSet: created." << endl;  
   }
   virtual ~testAppDialogSet() 
   {  
      cout << mSampleAppData << ": testAppDialogSet: destroyed." << endl;  
   }
   virtual AppDialog* createAppDialog(const SipMessage& msg) 
   {  
      return new testAppDialog(mDum, mSampleAppData);  
   }
   virtual UserProfile* selectUASUserProfile(const SipMessage& msg) 
   { 
      cout << mSampleAppData << ": testAppDialogSet: UAS UserProfile requested for msg: " << msg.brief() << endl;  
      return mDum.getMasterProfile(); 
   }
   Data mSampleAppData;
};

class testAppDialogSetFactory : public AppDialogSetFactory
{
public:
   virtual AppDialogSet* createAppDialogSet(DialogUsageManager& dum, const SipMessage& msg) 
   {  return new testAppDialogSet(dum, Data("UAS") + Data("(") + getMethodName(msg.header(h_RequestLine).getMethod()) + Data(")"));  }
   // For a UAS the testAppDialogSet will be created by DUM using this function.  If you want to set 
   // Application Data, then one approach is to wait for onNewSession(ServerInviteSessionHandle ...) 
   // to be called, then use the ServerInviteSessionHandle to get at the AppDialogSet or AppDialog,
   // then cast to your derived class and set the desired application data.
};


// Generic InviteSessionHandler
class TestInviteSessionHandler : public InviteSessionHandler, public ClientRegistrationHandler, public OutOfDialogHandler
{
   public:
      Data name;
      bool registered;
      ClientRegistrationHandle registerHandle;
      
      TestInviteSessionHandler(const Data& n) : name(n), registered(false) 
      {
      }

      virtual ~TestInviteSessionHandler()
      {
      }
      
      virtual void onSuccess(ClientRegistrationHandle h, const SipMessage& response)
      {         
         registerHandle = h;   
         assert(registerHandle.isValid());         
         cout << name << ": ClientRegistration-onSuccess - " << response.brief() << endl;
         registered = true;
      }

      virtual void onFailure(ClientRegistrationHandle, const SipMessage& msg)
      {
         cout << name << ": ClientRegistration-onFailure - " << msg.brief() << endl;
         throw;  // Ungracefully end
      }

      virtual void onRemoved(ClientRegistrationHandle)
      {
          cout << name << ": ClientRegistration-onRemoved" << endl;
      }

      virtual int onRequestRetry(ClientRegistrationHandle, int retrySeconds, const SipMessage& response)
      {
          cout << name << ": ClientRegistration-onRequestRetry (" << retrySeconds << ") - " << response.brief() << endl;
          return -1;
      }

      virtual void onNewSession(ClientInviteSessionHandle, InviteSession::OfferAnswerType oat, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onNewSession - " << msg.brief() << endl;
      }
      
      virtual void onNewSession(ServerInviteSessionHandle, InviteSession::OfferAnswerType oat, const SipMessage& msg)
      {
         cout << name << ": ServerInviteSession-onNewSession - " << msg.brief() << endl;
      }

      virtual void onFailure(ClientInviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onFailure - " << msg.brief() << endl;
      }
      
      virtual void onProvisional(ClientInviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onProvisional - " << msg.brief() << endl;
      }

      virtual void onConnected(ClientInviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onConnected - " << msg.brief() << endl;
      }

      virtual void onStaleCallTimeout(ClientInviteSessionHandle)
      {
         cout << name << ": ClientInviteSession-onStaleCallTimeout" << endl;
      }

      virtual void onConnected(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onConnected - " << msg.brief() << endl;
      }

      virtual void onRedirected(ClientInviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onRedirected - " << msg.brief() << endl;
      }

      virtual void onTerminated(InviteSessionHandle, InviteSessionHandler::TerminatedReason reason, const SipMessage* msg)
      {
         cout << name << ": InviteSession-onTerminated - " << (msg ? msg->brief() : "") << endl;
         assert(0); // This is overrideen in UAS and UAC specific handlers
      }

      virtual void onAnswer(InviteSessionHandle, const SipMessage& msg, const SdpContents& sdp)
      {
         cout << name << ": InviteSession-onAnswer(SDP)" << endl;
         //sdp->encode(cout);
      }

      virtual void onOffer(InviteSessionHandle is, const SipMessage& msg, const SdpContents& sdp)      
      {
         cout << name << ": InviteSession-onOffer(SDP)" << endl;
         //sdp->encode(cout);
      }

      virtual void onEarlyMedia(ClientInviteSessionHandle, const SipMessage& msg, const SdpContents& sdp)
      {
         cout << name << ": InviteSession-onEarlyMedia(SDP)" << endl;
         //sdp->encode(cout);
      }

      virtual void onOfferRequired(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onOfferRequired - " << msg.brief() << endl;
      }

      virtual void onOfferRejected(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onOfferRejected - " << msg.brief() << endl;
      }

      virtual void onDialogModified(InviteSessionHandle, InviteSession::OfferAnswerType oat, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onDialogModified - " << msg.brief() << endl;
      }

      virtual void onInfo(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onInfo - " << msg.brief() << endl;
      }

      virtual void onRefer(InviteSessionHandle, ServerSubscriptionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onRefer - " << msg.brief() << endl;
      }

      virtual void onReferAccepted(InviteSessionHandle, ClientSubscriptionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onReferAccepted - " << msg.brief() << endl;
      }

      virtual void onReferRejected(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onReferRejected - " << msg.brief() << endl;
      }

      virtual void onInfoSuccess(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onInfoSuccess - " << msg.brief() << endl;
      }

      virtual void onInfoFailure(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onInfoFailure - " << msg.brief() << endl;
      }

      virtual void onForkDestroyed(ClientInviteSessionHandle)
	  {
         cout << name << ": ClientInviteSession-onForkDestroyed" << endl;
	  }

      // Out-of-Dialog Callbacks
      virtual void onSuccess(ClientOutOfDialogReqHandle, const SipMessage& successResponse)
      {
          cout << name << ": ClientOutOfDialogReq-onSuccess - " << successResponse.brief() << endl;
      }
      virtual void onFailure(ClientOutOfDialogReqHandle, const SipMessage& errorResponse)
      {
          cout << name << ": ClientOutOfDialogReq-onFailure - " << errorResponse.brief() << endl;
      }
      virtual void onReceivedRequest(ServerOutOfDialogReqHandle ood, const SipMessage& request)
      {
          cout << name << ": ServerOutOfDialogReq-onReceivedRequest - " << request.brief() << endl;
          // Add SDP to response here if required
          cout << name << ": Sending 200 response to OPTIONS." << endl;
          ood->send(ood->answerOptions());
      }
};

class TestUac : public TestInviteSessionHandler
{
   public:
      bool done;
      SdpContents* sdp;     
      HeaderFieldValue* hfv;      
      Data* txt;      

      TestUac() 
         : TestInviteSessionHandler("UAC"), 
           done(false),
           sdp(0),
           hfv(0),
           txt(0)
      {
         txt = new Data("v=0\r\n"
                        "o=1900 369696545 369696545 IN IP4 192.168.2.15\r\n"
                        "s=X-Lite\r\n"
                        "c=IN IP4 192.168.2.15\r\n"
                        "t=0 0\r\n"
                        "m=audio 8000 RTP/AVP 8 3 98 97 101\r\n"
                        "a=rtpmap:8 pcma/8000\r\n"
                        "a=rtpmap:3 gsm/8000\r\n"
                        "a=rtpmap:98 iLBC\r\n"
                        "a=rtpmap:97 speex/8000\r\n"
                        "a=rtpmap:101 telephone-event/8000\r\n"
                        "a=fmtp:101 0-15\r\n");
         
         hfv = new HeaderFieldValue(txt->data(), txt->size());
         Mime type("application", "sdp");
         sdp = new SdpContents(hfv, type);
      }

      virtual ~TestUac()
      {
         delete sdp;
      }

      virtual void onTerminated(InviteSessionHandle, InviteSessionHandler::TerminatedReason reason, const SipMessage* msg)
      {
         cout << name << ": InviteSession-onTerminated - " << (msg ? msg->brief() : "") << endl;
         done = true;
      }
};

class TestUas : public TestInviteSessionHandler
{

   public:
      bool done;
      time_t* pHangupAt;

      SdpContents* sdp;
      HeaderFieldValue* hfv;
      Data* txt;      

      TestUas(time_t* pH) 
         : TestInviteSessionHandler("UAS"), 
           done(false),
           pHangupAt(pH),
           hfv(0)
      { 
         pHangupAt = pHangupAt;

         txt = new Data("v=0\r\n"
                        "o=1900 369696545 369696545 IN IP4 192.168.2.15\r\n"
                        "s=X-Lite\r\n"
                        "c=IN IP4 192.168.2.15\r\n"
                        "t=0 0\r\n"
                        "m=audio 8001 RTP/AVP 8 3 98 97 101\r\n"
                        "a=rtpmap:8 pcma/8000\r\n"
                        "a=rtpmap:3 gsm/8000\r\n"
                        "a=rtpmap:98 iLBC\r\n"
                        "a=rtpmap:97 speex/8000\r\n"
                        "a=rtpmap:101 telephone-event/8000\r\n"
                        "a=fmtp:101 0-15\r\n");
         
         hfv = new HeaderFieldValue(txt->data(), txt->size());
         Mime type("application", "sdp");
         sdp = new SdpContents(hfv, type);
      }

      ~TestUas()
      {
         delete sdp;
      }

      virtual void 
      onNewSession(ServerInviteSessionHandle sis, InviteSession::OfferAnswerType oat, const SipMessage& msg)
      {
         cout << name << ": ServerInviteSession-onNewSession - " << msg.brief() << endl;
         cout << name << ": Sending 180 response." << endl;
         mSis = sis;         
         sis->provisional(180);
      }

      virtual void onTerminated(InviteSessionHandle, InviteSessionHandler::TerminatedReason reason, const SipMessage* msg)
      {
         cout << name << ": InviteSession-onTerminated - " << (msg ? msg->brief() : "") << endl;
         done = true;
      }

      virtual void onOffer(InviteSessionHandle is, const SipMessage& msg, const SdpContents& sdp)      
      {
         cout << name << ": InviteSession-onOffer(SDP)" << endl;
         //sdp->encode(cout);
         cout << name << ": Sending 200 response with SDP answer." << endl;
         is->provideAnswer(sdp);
         mSis->accept();
         *pHangupAt = time(NULL) + 5;
      }

      // Normal people wouldn't put this functionality in the handler
      // it's a kludge for this test for right now
      void hangup()
      {
         if (mSis.isValid())
         {
            cout << name << ": Sending BYE." << endl;
            mSis->end();
            done = true;
         }
      }
   private:
      ServerInviteSessionHandle mSis;      
};

class TestShutdownHandler : public DumShutdownHandler
{
   public:
      TestShutdownHandler(const Data& n) : name(n), dumShutDown(false) { }
      virtual ~TestShutdownHandler(){}

      Data name;
      bool dumShutDown;
      virtual void onDumCanBeDeleted() 
      {
         cout << name << ": onDumCanBeDeleted." << endl;
         dumShutDown = true;
      }
};


#define NO_REGISTRATION 1
int 
main (int argc, char** argv)
{
   Log::initialize(Log::Cout, resip::Log::Warning, argv[0]);
   //Log::initialize(Log::Cout, resip::Log::Debug, argv[0]);
   //Log::initialize(Log::Cout, resip::Log::Info, argv[0]);

   //set up UAC
   SipStack stackUac;
   DialogUsageManager* dumUac = new DialogUsageManager(stackUac);
   dumUac->addTransport(UDP, 12005);

   MasterProfile uacMasterProfile;      
   auto_ptr<ClientAuthManager> uacAuth(new ClientAuthManager);
   dumUac->setMasterProfile(&uacMasterProfile);
   dumUac->setClientAuthManager(uacAuth);

   TestUac uac;
   dumUac->setInviteSessionHandler(&uac);
   dumUac->setClientRegistrationHandler(&uac);
   dumUac->addOutOfDialogHandler(OPTIONS, &uac);

   auto_ptr<AppDialogSetFactory> uac_dsf(new testAppDialogSetFactory);
   dumUac->setAppDialogSetFactory(uac_dsf);

#if !defined(NO_REGISTRATION)
   //your aor, credentials, etc here
   NameAddr uacAor("sip:101@foo.net");
   dumUac->getMasterProfile()->addDigestCredential( "foo.net", "derek@foo.net", "pass6" );
   dumUac->getMasterProfile()->setOutboundProxy(Uri("sip:209.134.58.33:9090"));    
#else
   NameAddr uacAor("sip:UAC@127.0.0.1:12005");
#endif

   dumUac->getMasterProfile()->setDefaultFrom(uacAor);
   dumUac->getMasterProfile()->setDefaultRegistrationTime(70);

   //set up UAS
   SipStack stackUas;
   DialogUsageManager* dumUas = new DialogUsageManager(stackUas);
   dumUas->addTransport(UDP, 12010);
   
   MasterProfile uasMasterProfile;   
   std::auto_ptr<ClientAuthManager> uasAuth(new ClientAuthManager);
   dumUas->setMasterProfile(&uasMasterProfile);
   dumUas->setClientAuthManager(uasAuth);

#if !defined(NO_REGISTRATION)
   //your aor, credentials, etc here
   NameAddr uasAor("sip:105@foo.net");
   dumUas->getMasterProfile()->addDigestCredential( "foo.net", "derek@foo.net", "pass6" );
   dumUas->getMasterProfile()->setOutboundProxy(Uri("sip:209.134.58.33:9090"));    
#else
   NameAddr uasAor("sip:UAS@127.0.0.1:12010");
#endif

   dumUas->getMasterProfile()->setDefaultRegistrationTime(70);
   dumUas->getMasterProfile()->setDefaultFrom(uasAor);

   time_t bHangupAt = 0;
   TestUas uas(&bHangupAt);
   dumUas->setClientRegistrationHandler(&uas);
   dumUas->setInviteSessionHandler(&uas);
   dumUas->addOutOfDialogHandler(OPTIONS, &uas);

   auto_ptr<AppDialogSetFactory> uas_dsf(new testAppDialogSetFactory);
   dumUas->setAppDialogSetFactory(uas_dsf);

#if !defined(NO_REGISTRATION)
   {
      SipMessage& regMessage = dumUas->makeRegistration(uasAor, new testAppDialogSet(*dumUac, "UAS(Registration)"));
      cout << "Sending register for Uas: " << endl << regMessage << endl;
      dumUas->send(regMessage);
   }
   {
      SipMessage& regMessage = dumUac->makeRegistration(uacAor, new testAppDialogSet(*dumUac, "UAS(Registration)"));
      cout << "Sending register for Uac: " << endl << regMessage << endl;
      dumUac->send(regMessage);
   }
#else
   uac.registered = true;
   uas.registered = true;
#endif

   bool finishedTest = false;
   bool stoppedRegistering = false;
   bool startedCallFlow = false;
   bool hungup = false;   
   TestShutdownHandler uasShutdownHandler("UAS");   
   TestShutdownHandler uacShutdownHandler("UAC");   

   while (!(uasShutdownHandler.dumShutDown && uacShutdownHandler.dumShutDown))
   {
     if (!uacShutdownHandler.dumShutDown)
     {
        FdSet fdset;
        stackUac.buildFdSet(fdset);
        int err = fdset.selectMilliSeconds(resipMin((int)stackUac.getTimeTillNextProcessMS(), 50));
        assert ( err != -1 );
        stackUac.process(fdset);
        while(dumUac->process());
     }
     if (!uasShutdownHandler.dumShutDown)
     {
        FdSet fdset;
        stackUas.buildFdSet(fdset);
        int err = fdset.selectMilliSeconds(resipMin((int)stackUas.getTimeTillNextProcessMS(), 50));
        assert ( err != -1 );
        stackUas.process(fdset);
        while(dumUas->process());
     }

     if (!(uas.done && uac.done))
     {
        if (uas.registered && uac.registered && !startedCallFlow)
        {
           if (!startedCallFlow)
           {
              startedCallFlow = true;
#if !defined(NO_REGISTRATION)
              cout << "!!!!!!!!!!!!!!!! Registered !!!!!!!!!!!!!!!! " << endl;
#endif

              // Kick off call flow by sending an OPTIONS request then an INVITE request from the UAC to the UAS
              cout << "UAC: Sending Options Request to UAS." << endl;
			  dumUac->send(dumUac->makeOutOfDialogRequest(uasAor, OPTIONS, new testAppDialogSet(*dumUac, "UAC(OPTIONS)")));  // Should probably add Allow, Accept, Accept-Encoding, Accept-Language and Supported headers - but this is fine for testing/demonstration

              cout << "UAC: Sending Invite Request to UAS." << endl;
              dumUac->send(dumUac->makeInviteSession(uasAor, uac.sdp, new testAppDialogSet(*dumUac, "UAC(INVITE)")));
           }
        }

        // Check if we should hangup yet
        if (bHangupAt!=0)
        {
           if (time(NULL)>bHangupAt && !hungup)
           {
              hungup = true;
              uas.hangup();
           }
        }
     }
     else
     {
        if (!stoppedRegistering)
        {
           finishedTest = true;
           stoppedRegistering = true;
           dumUas->shutdown(&uasShutdownHandler);
           dumUac->shutdown(&uacShutdownHandler);
#if !defined(NO_REGISTRATION)
           uas.registerHandle->stopRegistering();
           uac.registerHandle->stopRegistering();
#endif
        }
     }
   }

   // OK to delete DUM objects now
   delete dumUac; 
   delete dumUas;

   cout << "!!!!!!!!!!!!!!!!!! Successful !!!!!!!!!! " << endl;
}

/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */