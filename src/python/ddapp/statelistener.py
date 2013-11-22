from ddapp import lcmUtils
from ddapp import transformUtils
import PythonQt
import bot_core as lcmbotcore

from ddapp.simpletimer import SimpleTimer

class StateListener(object):

    def __init__(self):
        self.subscriber = None
        self.transforms = []
        self.timer = SimpleTimer()
        self.paused = True



    def onBDIPose(self, messageData):
        t = transformUtils.transformFromPose(m.pos, m.orientation)
        if self.timer.elapsed() > 1.0:
            self.transforms.append((self.timer.now(), t))
            self.timer.reset()


    def describe(self):

        print '----------'
        print '%d transforms' % len(self.transforms)
        if not self.transforms:
            return

        time0, transform0 = self.transforms[0]
        o0 = np.array(transform0.GetOrientation())
        p0 = np.array(transform0.GetPosition())
        for timeN, transformN in self.transforms:
            oN = np.array(transformN.GetOrientation())
            pN = np.array(transformN.GetPosition())
            print '%.2f:' % (timeN - time0), oN - o0, pN - p0

    def init(self):
        self.subscriber = lcmUtils.addSubscriber('POSE_BDI', lcmbotcore.pose_t, self.onBDIPose)


listener = StateListener()
listener.init()
