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
        public delegate string[] SelectCounter(string deviceId, string[] counters);
        public delegate void ReceivedValues(string deviceId, float[] values);

        private SelectCounter selectCounters;
        private ReceivedValues receivedValues;

        public void StartServer(SelectCounter selectCountersDelegate, ReceivedValues receivedValuesDelegate)
        {
            selectCounters = selectCountersDelegate;
            receivedValues = receivedValuesDelegate;
            System.Security.Principal.SecurityIdentifier sid = new System.Security.Principal.SecurityIdentifier(System.Security.Principal.WellKnownSidType.WorldSid, null);
            PipeAccessRule pac = new PipeAccessRule(sid, PipeAccessRights.ReadWrite, System.Security.AccessControl.AccessControlType.Allow);
            PipeSecurity ps = new PipeSecurity();
            ps.AddAccessRule(pac);
            sid = null;
            pipeServer = new NamedPipeServerStream(serverPipeName,
                    PipeDirection.InOut,
                    serverInstances,
                    PipeTransmissionMode.Message,
                    PipeOptions.Asynchronous,
                    1024,
                    1024,
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
            try
            {
                char[] data = new char[1024];
                pipeServer.EndWaitForConnection(Result);
                StreamReader reader = new StreamReader(pipeServer);
                StreamWriter writer = new StreamWriter(pipeServer);

                reader.ReadBlock(data, 0, 1024);
                MonitorMessage messageIN = MonitorMessage.ParseFromString(new String(data));
                Console.WriteLine("server received message " + messageIN.ToString());
                if (messageIN.As == 0 && messageIN.Body.Length>0)
                {
                    // TODO: check return code is zero
                    // create pipe name
                    string newPipeName = CreatePipeName(messageIN.Body[0]);
                    Console.WriteLine("creating pipe for device " + messageIN.Body[0]);
                    NewQueueToken token = new NewQueueToken() 
                        {DeviceId=messageIN.Body[0], 
                            queueName=newPipeName,
                        selectCounter = selectCounters,
                        receivedValues = receivedValues};
                    // create thread
                    ThreadPool.QueueUserWorkItem(new WaitCallback(HandleQueueThread), token);
                    if (token.semaphore.WaitOne(new TimeSpan(0, 0, 5), false))
                    {
                        // send pipe name
                        MonitorMessage messageOUT = new MonitorMessage(OpCodes.OK_MESSAGE, 0, 0, newPipeName);
                        writer.Write(messageOUT.ToString());
                        writer.Flush();
                    }
                    else
                    {
                        // timeout, aborting
                        MonitorMessage messageOUT = new MonitorMessage(OpCodes.OK_MESSAGE, 1, 0);
                        writer.Write(messageOUT.ToString());
                        writer.Flush();
                        token.abort = true;
                    }   
                }
                else
                {
                    // received a message with error code
                    Console.WriteLine("return code {0}, error code {1}", messageIN.As, messageIN.Aps);
                }
                pipeServer.Disconnect();
                AsyncCallback myCallback = new AsyncCallback(AsyncPipeCallback);
                pipeServer.BeginWaitForConnection(myCallback, null);
            }
            catch (OperationCanceledException)
            {
                Console.WriteLine("Oops, exiting the thread that started the BeginWaitForConnection() on this pipe has cancelled BeginWaitForConnection().");
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
                    PipeTransmissionMode.Message,
                    PipeOptions.None,
                    1024,
                    1024,
                    ps))
            {                Console.WriteLine("pipe {0} created, waiting..", token.queueName);
                // inform the main thread that the queue is on
                token.semaphore.Set();
                if (token.abort)
                    return;
                queuePipe.WaitForConnection();
                Console.WriteLine("pipe {0} connected", token.queueName);
                StreamReader reader = new StreamReader(queuePipe);
                StreamWriter writer = new StreamWriter(queuePipe);
                try
                {
                    MonitorMessage messageIn;
                    // step 1: enable counters
                    char[] data = new char[1024];
                    for (int i = 0; i < data.Length; i++)
                        data[i] = (char)0;
                    reader.ReadBlock(data, 0, data.Length);
                    messageIn = MonitorMessage.ParseFromString(new String(data));
                    Console.WriteLine("pipe {0} received {1}", token.queueName, messageIn.ToString());

                    if (messageIn.OpCode != OpCodes.ENABLE_COUNTERS_MESSAGE)
                        throw new InvalidOperationException("I was expeting an Enumerate Counters message and received " + messageIn.OpCode.ToString());
                    // counters
                    List<string> countersList = new List<string>(messageIn.Body);
                    // list of counters to enable
                    // TODO: how to get selectedCounters?
                    if (token.selectCounter != null)
                    {
                        string[] selectedCounters = token.selectCounter(token.DeviceId, countersList.ToArray());
                        writer.Write(new MonitorMessage(OpCodes.OK_MESSAGE, 0, 0, selectedCounters.Select(x => countersList.IndexOf(x)).ToArray()).ToString());
                        writer.Flush();
                    }
                    else
                    {
                        // delegate not set!
                        writer.Write(new MonitorMessage(OpCodes.OK_MESSAGE, 0, 0, new int[] { }).ToString());
                        writer.Flush();
                    }
                    // step 2:perf init
                    for (int i = 0; i < data.Length; i++)
                        data[i] = (char)0;
                    reader.ReadBlock(data, 0, data.Length);
                    messageIn = MonitorMessage.ParseFromString(new String(data));
                    Console.WriteLine("pipe {0} received {1}", token.queueName, messageIn.ToString());
                    if (messageIn.OpCode != OpCodes.GPU_PERF_INIT_MESSAGE)
                        throw new InvalidOperationException("I was expeting an Perf init message and received " + messageIn.OpCode.ToString());
                    writer.Write(new MonitorMessage(OpCodes.OK_MESSAGE, 0, 0).ToString());
                    writer.Flush();

                    // step 3: release
                    for (int i = 0; i < data.Length; i++)
                        data[i] = (char)0;
                    reader.ReadBlock(data, 0, data.Length);
                    messageIn = MonitorMessage.ParseFromString(new String(data));
                    if (messageIn.OpCode != OpCodes.RELEASE_QUEUE_MESSAGE)
                        throw new InvalidOperationException("I was expeting a release message and received " + messageIn.OpCode.ToString());
                    writer.Write(new MonitorMessage(OpCodes.OK_MESSAGE, 0, 0).ToString());
                    writer.Flush();

                    // step 5: end
                    for (int i = 0; i < data.Length; i++)
                        data[i] = (char)0;
                    reader.ReadBlock(data, 0, data.Length);
                    messageIn = MonitorMessage.ParseFromString(new String(data));
                    Console.WriteLine("pipe {0} received {1}", token.queueName, messageIn.ToString());
                    if (messageIn.OpCode != OpCodes.GPU_PERF_RELEASE_MESSAGE)
                        throw new InvalidOperationException("I was expeting an End message and received " + messageIn.OpCode.ToString());
                    writer.Write(new MonitorMessage(OpCodes.OK_MESSAGE, 0, 0).ToString());
                    writer.Flush();

                    // step 4: get counters
                    for (int i = 0; i < data.Length; i++)
                        data[i] = (char)0;
                    reader.ReadBlock(data, 0, data.Length);
                    messageIn = MonitorMessage.ParseFromString(new String(data));
                    Console.WriteLine("pipe {0} received {1}", token.queueName, messageIn.ToString());
                    if (messageIn.OpCode != OpCodes.GET_COUNTERS_MESSAGE)
                        throw new InvalidOperationException("I was expeting a Get Counters message and received " + messageIn.OpCode.ToString());
                    float[] values = messageIn.BodyAsFloatArray;
                    // TODO: send values
                    if (token.receivedValues != null)
                    {
                        token.receivedValues(token.DeviceId, values);
                    }

                    writer.Write(new MonitorMessage(OpCodes.OK_MESSAGE, 0, 0).ToString());
                    writer.Flush();


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
                    writer.Write(new MonitorMessage(OpCodes.OK_MESSAGE, 1, 1).ToString());
                    writer.Flush();
                }
                finally
                {
                    queuePipe.Close();
                }
            }
        }
 
    }



}
