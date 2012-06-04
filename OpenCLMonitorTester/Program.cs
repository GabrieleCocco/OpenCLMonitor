using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using OpenCLDotNetMonitor;
using System.IO.Pipes;
using System.Threading;

namespace OpenCLMonitorTester
{
    class Program
    {
        static void Main(string[] args)
        {
            OpenCLMonitorServer server = new OpenCLMonitorServer();
            server.StartServer(new OpenCLMonitorServer.SelectCounter(SelectCounters), new OpenCLMonitorServer.ReceivedValues(ReceivedValues));
            Console.WriteLine("waiting for data");
            ThreadPool.QueueUserWorkItem(new WaitCallback(TestClient), new object());
            Console.WriteLine("test program running, press enter to stop");
            Console.ReadLine();
            server.StopServer();
        }

        static void ReceivedValues(string device, float[] values)
        {
            StringBuilder sb = new StringBuilder();
            sb.AppendFormat("TestClient received values.. for device {0}:", device);
            foreach (float v in values)
            {
                sb.AppendFormat("{0},", v);
            }
            Console.WriteLine(sb.ToString());
        }

        static string[] SelectCounters(string device, string[] counters)
        {
            Console.WriteLine("selecting counters for device {0}", device);
            return new string[] { counters[0]};
        }

        static void TestClient(object stateInfo)
        {
            Console.WriteLine("entering TestClient");
            NamedPipeClientStream pipeClient1 =
                new NamedPipeClientStream(".", "openclmonitor_pipe",
            PipeDirection.InOut, PipeOptions.None);

            pipeClient1.Connect(5000);

            if (!pipeClient1.IsConnected)
            {
                Console.WriteLine("TestClient KO;Could not connect to pipe");
                return;
            }

            StreamString ss = new StreamString(pipeClient1);

            MonitorMessage createMsgOUT = new MonitorMessage(OpCodes.CREATE, 0, 0, "DEVICETEST");
            ss.WriteString(createMsgOUT.ToString());
            MonitorMessage createMsgIN = MonitorMessage.ParseFromString(ss.ReadString());
            Console.WriteLine("TestClient Received {0}", createMsgIN.ToString());
            pipeClient1.Close();


            NamedPipeClientStream pipeClient2 =
                new NamedPipeClientStream(".", createMsgIN.Body[0],
                PipeDirection.InOut, PipeOptions.None);

            pipeClient2.Connect(5000);

            if (!pipeClient2.IsConnected)
            {
                Console.WriteLine("TestClient KO;Could not connect to queue pipe");
                return;
            }

            ss = new StreamString(pipeClient2);

            MonitorMessage enumOUT = new MonitorMessage(OpCodes.ENUMERATE_COUNTERS, 0, 0, new string[] { "counterA", "counterB" });
            ss.WriteString(enumOUT.ToString());
            MonitorMessage enumIN = MonitorMessage.ParseFromString(ss.ReadString());
            Console.WriteLine("TestClient Received {0}", enumIN.ToString());
            StringBuilder sb = new StringBuilder();
            sb.Append("TestClient received enable counters: ");
            foreach (int cid in enumIN.BodyAsFloatArray)
            {
                sb.AppendFormat("{0}, ", cid);
            }
            Console.WriteLine(sb.ToString());

            {
                MonitorMessage mOut = new MonitorMessage(OpCodes.PERF_INIT, 0, 0);
                ss.WriteString(mOut.ToString());
                MonitorMessage mIn = MonitorMessage.ParseFromString(ss.ReadString());
                Console.WriteLine("TestClient Received {0}", mIn.ToString());
            }

            {
                MonitorMessage mOut = new MonitorMessage(OpCodes.RELEASE, 0, 0);
                ss.WriteString(mOut.ToString());
                MonitorMessage mIn = MonitorMessage.ParseFromString(ss.ReadString());
                Console.WriteLine("TestClient Received {0}", mIn.ToString());
            }

            {
                MonitorMessage mOut = new MonitorMessage(OpCodes.GET_COUNTERS, 0, 0, new float[]{1.1f, 2.2f});
                ss.WriteString(mOut.ToString());
                MonitorMessage mIn = MonitorMessage.ParseFromString(ss.ReadString());
                Console.WriteLine("TestClient Received {0}", mIn.ToString());
            }

            {
                MonitorMessage mOut = new MonitorMessage(OpCodes.END, 0, 0);
                ss.WriteString(mOut.ToString());
                MonitorMessage mIn = MonitorMessage.ParseFromString(ss.ReadString());
                Console.WriteLine("TestClient Received {0}", mIn.ToString());
            }

            pipeClient2.Close();
            Console.WriteLine("TestClient queue done");

        }
    }
}
