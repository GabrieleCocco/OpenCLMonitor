using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace OpenCLDotNetMonitor
{
    public class NewQueueToken
    {
        public string DeviceId { get; set; }
        public string queueName { get; set; }
        public AutoResetEvent semaphore = new AutoResetEvent(false);
        public bool abort = false;
    }
}
