using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using OpenCLDotNetMonitor;

namespace OpenCLMonitorTester
{
    class Program
    {
        static void Main(string[] args)
        {
            OpenCLMonitorServer server = new OpenCLMonitorServer();
            server.StartServer(new OpenCLMonitorServer.SelectCounter(SelectCounters), new OpenCLMonitorServer.ReceivedValues(ReceivedValues));
            Console.WriteLine("waiting for data, press enter to stop");
            Console.ReadLine();
            server.StopServer();
        }

        static void ReceivedValues(string device, float[] values)
        {
            StringBuilder sb = new StringBuilder();
            sb.AppendFormat("{0}:", device);
            foreach (float v in values)
            {
                sb.AppendFormat("{0},", v);
            }
            Console.WriteLine(sb.ToString());
        }

        static string[] SelectCounters(string device, string[] counters)
        {
            return new string[] { counters[0]};
        }
    }
}
