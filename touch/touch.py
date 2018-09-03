#!/usr/bin/env python3
import numpy
import cv2
import logging

class TouchCapture:
        def __init__(self):
            self.initialize_detector_params()
            self.detector = cv2.SimpleBlobDetector_create(self.params)

        def open(self, camera):
            self.cap = cv2.VideoCapture(camera)
            # The Ailipu camera I used runs fastest at VGA
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

            self.flush_frames()
            self.reset_background(10)
            self.update_frame()

        def get_frame(self):
            ret, image = self.cap.read()
            return cv2.cvtColor(image,cv2.COLOR_BGR2GRAY)   

        def flush_frames(self):
            # Flush some frames
            for i in range(15):
                self.get_frame()

        # Create blob detector parameters hand tuned for this use case
        def initialize_detector_params(self):
            self.params = cv2.SimpleBlobDetector_Params()
            self.params.filterByCircularity = True
            self.params.minCircularity = 0.4
            logging.debug("Circularity %d %d", self.params.minCircularity, self.params.maxCircularity)

            self.params.filterByConvexity = True
            self.params.minConvexity = 0.4
            logging.debug("Convexity %d %d", self.params.minConvexity, self.params.maxConvexity)

            self.params.filterByInertia = True
            logging.debug("Inertia %d %d", self.params.minInertiaRatio, self.params.maxInertiaRatio)

            self.params.filterByArea = True
            self.params.minArea = 10
            self.params.maxArea = 500
            logging.debug("Area %d %d", self.params.minArea, self.params.maxArea)

            self.params.minDistBetweenBlobs = 3.0
            logging.debug("minDistBetweenBlobs %d", self.params.minDistBetweenBlobs)

            self.params.minRepeatability = 2
            logging.debug("minRepeatability %d", self.params.minRepeatability)

            self.params.blobColor = 255
            self.params.minThreshold = 10
            self.params.maxThreshold = 60
            self.params.thresholdStep = 5
            logging.debug("Threshold %d %d %d", self.params.minThreshold, self.params.maxThreshold, self.params.thresholdStep)

        # Average a specified number of frames to mitigate noise
        def reset_background(self, num_frames):
            image = self.get_frame()

            image = numpy.float32(image)/255.0
            average = image.copy()

            for i in range(num_frames):
                image = self.get_frame()

                image = numpy.float32(image)/255.0
                cv2.accumulateWeighted(image, average, 1.0/num_frames)

            self.background = numpy.uint8(average*255.0)

        def update_frame(self):
            # Capture frame-by-frame
            self.frame = self.get_frame()
            self.frame = cv2.subtract(self.frame, self.background)
            return self.frame

        def get_points(self):
            # Update the blob detector
            return self.detector.detect(self.frame)

        def close(self):
            self.cap.release()


if __name__ == "__main__":

    logging.basicConfig(level=logging.DEBUG)

    capture = TouchCapture()
    capture.open(1)
    frame = capture.update_frame()
    tick = cv2.getTickCount()

    while(True):
        frame = capture.update_frame()
        
        keypoints = capture.get_points()
        frame = cv2.drawKeypoints(frame, keypoints, numpy.array([]), (0,0,255), cv2.DRAW_MATCHES_FLAGS_DRAW_RICH_KEYPOINTS)

        # Display the resulting frame
        cv2.imshow('frame', frame)

        # Time the loop
        new_tick = cv2.getTickCount()
        t = (new_tick-tick)/cv2.getTickFrequency()
        tick = new_tick
        logging.debug("t {:05.3f} fps {:05.2f}".format(t, 1.0/t))

        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break
        elif key == ord('b'):
            # Reset the background image
            capture.reset_background(10)

    # When everything done, release the capture
    capture.close()
    cv2.destroyAllWindows()
