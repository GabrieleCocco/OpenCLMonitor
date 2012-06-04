﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO.Pipes;
using System.IO;
using System.Threading;

namespace OpenCLDotNetMonitor
{
    public class OpenCLMonitorServer
    {
        private static bool shouldStop = false;
        private static int serverInstances = 1;
        NamedPipeServerStream pipeServer;

        private const string serverPipeName = "openclmonitor_pipe";

        public void StartServer()
        {
            System.Security.Principal.SecurityIdentifier sid = new System.Security.Principal.SecurityIdentifier(System.Security.Principal.WellKnownSidType.WorldSid, null);
            PipeAccessRule pac = new PipeAccessRule(sid, PipeAccessRights.ReadWrite, System.Security.AccessControl.AccessControlType.Allow);
            PipeSecurity ps = new PipeSecurity();
            ps.AddAccessRule(pac);
            sid = null;
            pipeServer = new NamedPipeServerStream(serverPipeName,
                    PipeDirection.InOut,
                    serverInstances,
                    PipeTransmissionMode.Byte,
                    PipeOptions.Asynchronous,
                    1,
                    1,
                    ps);
            AsyncCallback myCallback = new AsyncCallback(AsyncPipeCallback);
            pipeServer.BeginWaitForConnection(myCallback, null);
        }

        public void StopServer()
        {
            if (pipeServer.IsConnected)
                pipeServer.Disconnect();
            pipeServer.Dispose();
        }

        /// <summary>
        /// creates an unique pipe name
        /// </summary>
        /// <returns></returns>
        private string CreatePipeName(string deviceID)
        {
            //return System.Guid.NewGuid().ToString("N");
            return string.Concat(new string[]{serverPipeName, "_", deviceID});
        }

        private void AsyncPipeCallback(IAsyncResult Result)
        {
            Console.WriteLine("AsyncPipeCallback Entered.");
            try
            {
                pipeServer.EndWaitForConnection(Result);
                StreamString ss = new StreamString(pipeServer);
                MonitorMessage messageIN = MonitorMessage.ParseFromString(ss.ReadString());
                if (messageIN.As == 0 && messageIN.Body.Length>0)
                {
                    // TODO: check return code is zero
                    // create pipe name
                    string newPipeName = CreatePipeName(messageIN.Body[0]);
                    NewQueueToken token = new NewQueueToken() {DeviceId=messageIN.Body[0], queueName=newPipeName };
                    // create thread
                    ThreadPool.QueueUserWorkItem(new WaitCallback(HandleQueueThread), token);
                    // TODO: wait for the new pipe server to be ready...
                    // LOCK ?
                    if (token.semaphore.WaitOne(new TimeSpan(0, 0, 5), false))
                    {
                        // send pipe name
                        MonitorMessage messageOUT = new MonitorMessage(OpCodes.OK, 0, 0, newPipeName);
                        ss.WriteString(messageOUT.ToString());
                    }
                    else
                    {
                        // timeout, aborting
                        MonitorMessage messageOUT = new MonitorMessage(OpCodes.OK, 1, 0);
                        ss.WriteString(messageOUT.ToString());
                        token.abort = true;
                    }

      
        }
                else
                {
                    // received a message with error code
                    Console.WriteLine("return code {0}, error code {1}", messageIN.As, messageIN.Aps);
                }
            }
            catch (OperationCanceledException)
            {
                Console.WriteLine("Oops, exiting the thread that started the BeginWaitForConnection() on this pipe has cancelled BeginWaitForConnection().");
                Console.WriteLine("WHY???");
            }
        }

        static void HandleQueueThread(Object stateInfo)
        {
            // the pipe here will be handled synchronously
            NewQueueToken token = (NewQueueToken)stateInfo;
            string pipename = token.queueName;
            System.Security.Principal.SecurityIdentifier sid = new System.Security.Principal.SecurityIdentifier(System.Security.Principal.WellKnownSidType.WorldSid, null);
            PipeAccessRule pac = new PipeAccessRule(sid, PipeAccessRights.ReadWrite, System.Security.AccessControl.AccessControlType.Allow);
            PipeSecurity ps = new PipeSecurity();
            ps.AddAccessRule(pac);
            sid = null;

            using (NamedPipeServerStream queuePipe = new NamedPipeServerStream(pipename,
                    PipeDirection.InOut,
                    serverInstances,
                    PipeTransmissionMode.Byte,
                    PipeOptions.None,
                    1,
                    1,
                    ps))
            {
                // inform the main thread that the queue is on
                token.semaphore.Set();
                if (token.abort)
                    return;
                queuePipe.WaitForConnection();
                StreamString ss = new StreamString(queuePipe);
                try
                {
                    MonitorMessage messageIn;
                    // step 1: enable counters
                    messageIn = MonitorMessage.ParseFromString(ss.ReadString());
                    if (messageIn.OpCode != OpCodes.ENUMERATE_COUNTERS)
                        throw new InvalidOperationException("I was expeting an Enumerate Counters message and received " + messageIn.OpCode.ToString());
                    // counters
                    List<string> countersList = new List<string>(messageIn.Body);
                    // list of counters to enable
                    // TODO: how to get selectedCounters?
                    string [] selectedCounters = new string[] {};
                    ss.WriteString(new MonitorMessage(OpCodes.OK, 0, 0, selectedCounters.Select(x => countersList.IndexOf(x)).ToArray()).ToString());
                    // step 2:perf init
                    messageIn = MonitorMessage.ParseFromString(ss.ReadString());
                    if (messageIn.OpCode != OpCodes.PERF_INIT)
                        throw new InvalidOperationException("I was expeting an Perf init message and received " + messageIn.OpCode.ToString());
                    ss.WriteString(new MonitorMessage(OpCodes.OK, 0, 0).ToString());
                    // step 3: release
                    messageIn = MonitorMessage.ParseFromString(ss.ReadString());
                    if (messageIn.OpCode != OpCodes.RELEASE)
                        throw new InvalidOperationException("I was expeting a release message and received " + messageIn.OpCode.ToString());
                    ss.WriteString(new MonitorMessage(OpCodes.OK, 0, 0).ToString());

                    // step 4: get counters
                    messageIn = MonitorMessage.ParseFromString(ss.ReadString());
                    if (messageIn.OpCode != OpCodes.GET_COUNTERS)
                        throw new InvalidOperationException("I was expeting a Get Counters message and received " + messageIn.OpCode.ToString());
                    float[] values = messageIn.BodyAsFloatArray;
                    // TODO: send values

                    ss.WriteString(new MonitorMessage(OpCodes.OK, 0, 0).ToString());

                    // step 5: end
                    messageIn = MonitorMessage.ParseFromString(ss.ReadString());
                    if (messageIn.OpCode != OpCodes.END)
                        throw new InvalidOperationException("I was expeting an End message and received " + messageIn.OpCode.ToString());
                    ss.WriteString(new MonitorMessage(OpCodes.OK, 0, 0).ToString());

                }
                // Catch the IOException that is raised if the pipe is broken
                // or disconnected.
                catch (IOException e)
                {
                    Console.WriteLine("ERROR: {0}", e.Message);
                }
                catch (InvalidOperationException e)
                {
                    // TODO: what is the code for invalid operation?
                    ss.WriteString(new MonitorMessage(OpCodes.OK, 1, 1).ToString());
                }
                finally
                {
                    queuePipe.Close();
                }
            }
        }
 
    }



}
