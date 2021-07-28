package com.picovr.picovr_treasurehunter;

import android.opengl.GLES30;
import android.opengl.Matrix;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.WindowManager;

import com.picovr.vractivity.Eye;
import com.picovr.vractivity.HmdState;
import com.picovr.vractivity.RenderInterface;
import com.picovr.vractivity.VRActivity;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

public class TreasureHunter extends VRActivity implements RenderInterface {

    private static final String TAG_TH = "PVRTreasureHunter";
    private static final String DBG_LC = "LifeCycle :";
    private static final int UER_EVENT = 100;
    private static final float Z_NEAR = 0.1f;
    private static final float Z_FAR = 1000.0f;
    private static final float CAMERA_Z = 0.01f;
    private static final float TIME_DELTA = 1.3f;
    private static final float YAW_LIMIT = 0.12f;
    private static final float PITCH_LIMIT = 0.12f;
    // We keep the light always position just above the user.
    private static final float[] LIGHT_POS_IN_WORLD_SPACE = new float[]{0.0f, 2.0f, 0.0f, 1.0f};
    // Convenience vector for extracting the position from a matrix via multiplication.
    private static final float[] POS_MATRIX_MULTIPLY_VEC = {0, 0, 0, 1.0f};
    private static final float MAX_MODEL_DISTANCE = 17.0f;
    final int FLOOR_VERTEX_POS_SIZE = 3;    // x, y, z
    final int FLOOR_VERTEX_COLOR_SIZE = 4;  // r, g, b, a
    final int FLOOR_VERTEX_NORMAL_SIZE = 3; // nx, ny, nz
    final int FLOOR_VERTEX_POS_INDEX = 0;
    final int FLOOR_VERTEX_COLOR_INDEX = 1;
    final int FLOOR_VERTEX_NORMAL_INDEX = 2;
    final int FLOOR_VERTEX_STRIDE = (4 * (FLOOR_VERTEX_POS_SIZE + FLOOR_VERTEX_COLOR_SIZE + FLOOR_VERTEX_NORMAL_SIZE));
    private final float[] lightPosInEyeSpace = new float[4];
    protected float[] modelCube;
    protected float[] modelCubeController;
    protected float[] modelPosition;
    protected float[] modelBtn;
    float[] poseData = new float[7];
    long[] poseTime = new long[3];
    private FloatBuffer cube_General_Vertics;
    private FloatBuffer cube_Found_Vertics;
    private ShortBuffer cube_Indexs;
    private FloatBuffer floor_Vertics;
    private ShortBuffer floor_Indexs;
    private int screenWidth;
    private int screenHeight;
    private int cubeProgram;
    private int floorProgram;
    private int[] cubeGeneralVBOIds = new int[2];
    private int[] cubeGeneralVAOId = new int[1];
    private int[] cubeFoundVBOIds = new int[2];
    private int[] cubeFoundVAOId = new int[1];
    private int[] floorVBOIds = new int[2];
    private int[] floorVAOId = new int[1];
    private int cubeModelParam;
    private int cubeModelViewParam;
    private int cubeModelViewProjectionParam;
    private int cubeLightPosParam;
    private int floorModelParam;
    private int floorModelViewParam;
    private int floorModelViewProjectionParam;
    private int floorLightPosParam;
    private float[] camera;
    private float[] view;
    private float[] headView;
    private float[] headOritation;
    private float[] modelViewProjection;
    private float[] modelView;
    private float[] modelFloor;
    private float[] tempPosition;
    private float floorDepth = 40f;
    // Add by Enoch for : calculate the FPS and log it.
    // Add by Enoch for : calculate the FPS and log it.
    private int mFPS;
    private long mTime0;

    @Override
    public void onFrameBegin(HmdState headTransform) {
//        Log.d(TAG_TH, DBG_LC + "onFrameBegin BEGIN");
        setCubeRotation();

        Matrix.setLookAtM(camera, 0, 0.0f, 0.0f, CAMERA_Z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

        float[] ori = headTransform.getOrientation();
        float[] pos = headTransform.getPos();
        quternion2Matrix(ori, headView);
        Matrix.translateM(headView, 0, pos[0], pos[1], pos[2]);
        checkGLError("onReadyToDraw");
    }

    @Override
    public void onDrawEye(Eye eye) {
//        Log.d(TAG_TH, DBG_LC + "onDrawEye BEGIN");

        GLES30.glViewport(0, 0, screenWidth, screenHeight);
        GLES30.glEnable(GLES30.GL_SCISSOR_TEST);
        GLES30.glDisable(GLES30.GL_CULL_FACE);
        GLES30.glScissor(1, 1, screenWidth - 2, screenHeight - 2);
        GLES30.glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT | GLES30.GL_DEPTH_BUFFER_BIT);

        Matrix.multiplyMM(view, 0, headView, 0, camera, 0);
        Matrix.multiplyMV(lightPosInEyeSpace, 0, view, 0, LIGHT_POS_IN_WORLD_SPACE, 0);

        float[] perspective = new float[16];
        getPerspective(Z_NEAR, Z_FAR, 51.f, 51.f, 51.f, 51.f, perspective, 0);

        Matrix.multiplyMM(modelView, 0, view, 0, modelCube, 0);
        Matrix.multiplyMM(modelViewProjection, 0, perspective, 0, modelView, 0);

        drawCube();

        Matrix.multiplyMM(modelView, 0, view, 0, modelFloor, 0);
        Matrix.multiplyMM(modelViewProjection, 0, perspective, 0, modelView, 0);

        drawFloor();

        GLES30.glFinish();
//        Log.d(TAG_TH, DBG_LC + "onDrawEye END");
    }

    @Override
    public void onFrameEnd() {
//        Log.d(TAG_TH, DBG_LC +" onFrameEnd CALLED");
        // Add by Enoch for : calculate FPS
        long time1 = System.currentTimeMillis();
        mFPS++;
        if (time1 - mTime0 > 1000) {
            mFPS = mFPS * 1000 / (int) (time1 - mTime0);
            Log.d(TAG_TH, "FPS:::::::::" + mFPS);
            mTime0 = time1;
            mFPS = 1;
        }
        // Add by Enoch for : calculate FPS
    }

    @Override
    public void onTouchEvent() {
//        Log.d(TAG_TH, DBG_LC +" onTouchEvent");
    }

    @Override
    public void onRenderPause() {
        //Log.d(TAG_TH, DBG_LC +" onRenderPause CALLED");
    }

    @Override
    public void onRenderResume() {
        //Log.d(TAG_TH, DBG_LC +" onRenderResume CALLED");
    }

    @Override
    public void onRendererShutdown() {
        Log.d(TAG_TH, DBG_LC + " onRendererShutdown CALLED");
    }

    @Override
    public void initGL(int w, int h) {
//        Log.d(TAG_TH, DBG_LC + "initGL BEGIN");
        screenHeight = h;
        screenWidth = w;

        cubeProgram = loadProgram(R.raw.light_vertex, R.raw.passthrough_fragment);
        checkGLError("Cube program");

        cubeModelParam = GLES30.glGetUniformLocation(cubeProgram, "u_Model");
        cubeModelViewParam = GLES30.glGetUniformLocation(cubeProgram, "u_MVMatrix");
        cubeModelViewProjectionParam = GLES30.glGetUniformLocation(cubeProgram, "u_MVP");
        cubeLightPosParam = GLES30.glGetUniformLocation(cubeProgram, "u_LightPos");
        checkGLError("Cube program params");

        GLES30.glGenBuffers(2, cubeGeneralVBOIds, 0);

        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, cubeGeneralVBOIds[0]);
        cube_General_Vertics.position(0);
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, WorldLayoutData.CUBE_GENERAL.length * 4,
                cube_General_Vertics, GLES30.GL_STATIC_DRAW);

        GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, cubeGeneralVBOIds[1]);
        cube_Indexs.position(0);
        GLES30.glBufferData(GLES30.GL_ELEMENT_ARRAY_BUFFER, 2 * WorldLayoutData.CUBE_INDEX.length,
                cube_Indexs, GLES30.GL_STATIC_DRAW);

        // Generate VAO Id
        GLES30.glGenVertexArrays(1, cubeGeneralVAOId, 0);
        // Bind the VAO and then setup the vertex attributes
        GLES30.glBindVertexArray(cubeGeneralVAOId[0]);
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, cubeGeneralVBOIds[0]);
        GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, cubeGeneralVBOIds[1]);

        GLES30.glEnableVertexAttribArray(FLOOR_VERTEX_POS_INDEX);
        GLES30.glEnableVertexAttribArray(FLOOR_VERTEX_COLOR_INDEX);
        GLES30.glEnableVertexAttribArray(FLOOR_VERTEX_NORMAL_INDEX);
        GLES30.glVertexAttribPointer(FLOOR_VERTEX_POS_INDEX, FLOOR_VERTEX_POS_SIZE,
                GLES30.GL_FLOAT, false, FLOOR_VERTEX_STRIDE,
                0);
        GLES30.glVertexAttribPointer(FLOOR_VERTEX_COLOR_INDEX, FLOOR_VERTEX_COLOR_SIZE,
                GLES30.GL_FLOAT, false, FLOOR_VERTEX_STRIDE,
                (FLOOR_VERTEX_POS_SIZE * 4));
        GLES30.glVertexAttribPointer(FLOOR_VERTEX_NORMAL_INDEX, FLOOR_VERTEX_NORMAL_SIZE,
                GLES30.GL_FLOAT, false, FLOOR_VERTEX_STRIDE,
                (FLOOR_VERTEX_POS_SIZE * 4 + FLOOR_VERTEX_COLOR_SIZE * 4));
        // Reset to the default VAO
        GLES30.glBindVertexArray(0);

        GLES30.glGenBuffers(2, cubeFoundVBOIds, 0);

        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, cubeFoundVBOIds[0]);
        cube_Found_Vertics.position(0);
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, WorldLayoutData.CUBE_FOUND.length * 4,
                cube_Found_Vertics, GLES30.GL_STATIC_DRAW);

        GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, cubeFoundVBOIds[1]);
        cube_Indexs.position(0);
        GLES30.glBufferData(GLES30.GL_ELEMENT_ARRAY_BUFFER, 2 * WorldLayoutData.CUBE_INDEX.length,
                cube_Indexs, GLES30.GL_STATIC_DRAW);

        // Generate VAO Id
        GLES30.glGenVertexArrays(1, cubeFoundVAOId, 0);
        // Bind the VAO and then setup the vertex attributes
        GLES30.glBindVertexArray(cubeFoundVAOId[0]);
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, cubeFoundVBOIds[0]);
        GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, cubeFoundVBOIds[1]);

        GLES30.glEnableVertexAttribArray(FLOOR_VERTEX_POS_INDEX);
        GLES30.glEnableVertexAttribArray(FLOOR_VERTEX_COLOR_INDEX);
        GLES30.glEnableVertexAttribArray(FLOOR_VERTEX_NORMAL_INDEX);
        GLES30.glVertexAttribPointer(FLOOR_VERTEX_POS_INDEX, FLOOR_VERTEX_POS_SIZE,
                GLES30.GL_FLOAT, false, FLOOR_VERTEX_STRIDE,
                0);
        GLES30.glVertexAttribPointer(FLOOR_VERTEX_COLOR_INDEX, FLOOR_VERTEX_COLOR_SIZE,
                GLES30.GL_FLOAT, false, FLOOR_VERTEX_STRIDE,
                (FLOOR_VERTEX_POS_SIZE * 4));
        GLES30.glVertexAttribPointer(FLOOR_VERTEX_NORMAL_INDEX, FLOOR_VERTEX_NORMAL_SIZE,
                GLES30.GL_FLOAT, false, FLOOR_VERTEX_STRIDE,
                (FLOOR_VERTEX_POS_SIZE * 4 + FLOOR_VERTEX_COLOR_SIZE * 4));
        // Reset to the default VAO
        GLES30.glBindVertexArray(0);

        floorProgram = loadProgram(R.raw.light_vertex, R.raw.grid_fragment);
        checkGLError("Floor program");

        floorModelParam = GLES30.glGetUniformLocation(floorProgram, "u_Model");
        floorModelViewParam = GLES30.glGetUniformLocation(floorProgram, "u_MVMatrix");
        floorModelViewProjectionParam = GLES30.glGetUniformLocation(floorProgram, "u_MVP");
        floorLightPosParam = GLES30.glGetUniformLocation(floorProgram, "u_LightPos");
        checkGLError("Floor program params");

        // Generate VBO Ids and load the VBOs with data
        GLES30.glGenBuffers(2, floorVBOIds, 0);

        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, floorVBOIds[0]);
        floor_Vertics.position(0);
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, WorldLayoutData.FLOOR.length * 4,
                floor_Vertics, GLES30.GL_STATIC_DRAW);

        GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, floorVBOIds[1]);
        floor_Indexs.position(0);
        GLES30.glBufferData(GLES30.GL_ELEMENT_ARRAY_BUFFER, 2 * WorldLayoutData.FLOOR_INDEX.length,
                floor_Indexs, GLES30.GL_STATIC_DRAW);

        // Generate VAO Id
        GLES30.glGenVertexArrays(1, floorVAOId, 0);
        // Bind the VAO and then setup the vertex attributes
        GLES30.glBindVertexArray(floorVAOId[0]);
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, floorVBOIds[0]);
        GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, floorVBOIds[1]);

        GLES30.glEnableVertexAttribArray(FLOOR_VERTEX_POS_INDEX);
        GLES30.glEnableVertexAttribArray(FLOOR_VERTEX_COLOR_INDEX);
        GLES30.glEnableVertexAttribArray(FLOOR_VERTEX_NORMAL_INDEX);
        GLES30.glVertexAttribPointer(FLOOR_VERTEX_POS_INDEX, FLOOR_VERTEX_POS_SIZE,
                GLES30.GL_FLOAT, false, FLOOR_VERTEX_STRIDE,
                0);
        GLES30.glVertexAttribPointer(FLOOR_VERTEX_COLOR_INDEX, FLOOR_VERTEX_COLOR_SIZE,
                GLES30.GL_FLOAT, false, FLOOR_VERTEX_STRIDE,
                (FLOOR_VERTEX_POS_SIZE * 4));
        GLES30.glVertexAttribPointer(FLOOR_VERTEX_NORMAL_INDEX, FLOOR_VERTEX_NORMAL_SIZE,
                GLES30.GL_FLOAT, false, FLOOR_VERTEX_STRIDE,
                (FLOOR_VERTEX_POS_SIZE * 4 + FLOOR_VERTEX_COLOR_SIZE * 4));
        // Reset to the default VAO
        GLES30.glBindVertexArray(0);

        Matrix.setIdentityM(modelFloor, 0);
        Matrix.translateM(modelFloor, 0, 0, -floorDepth, 0); // Floor appears below user.
        updateModelPosition();

//        Log.d(TAG_TH, DBG_LC + "initGL END");
    }

    @Override
    public void deInitGL() {
        Log.d(TAG_TH, DBG_LC + " deInitGL CALLED");
        GLES30.glDeleteProgram(cubeProgram);
        GLES30.glDeleteProgram(floorProgram);
        GLES30.glDeleteBuffers(2, cubeGeneralVBOIds, 0);
        GLES30.glDeleteVertexArrays(1, cubeGeneralVAOId, 0);
        GLES30.glDeleteBuffers(2, cubeFoundVBOIds, 0);
        GLES30.glDeleteVertexArrays(1, floorVAOId, 0);
        GLES30.glDeleteBuffers(2, floorVBOIds, 0);
    }

    @Override
    public void renderEventCallBack(int var) {
//        Log.d(TAG_TH, DBG_LC + "renderEventCallBack BEGIN");
        if (var == 101) {
            Log.d("", "UER_EVENT!!" + var);
        }

    }

    @Override
    public void surfaceChangedCallBack(int i, int i1) {

    }

    private int loadProgram(int vertex_resId, int fragment_resId) {
//        Log.d(TAG_TH, DBG_LC + "loadProgram BEGIN");
        int vertexShader;
        int fragmentShader;
        int programObject;
        int[] linked = new int[1];

        // Load the vertex/fragment shaders
        vertexShader = loadGLShader(GLES30.GL_VERTEX_SHADER, vertex_resId);

        if (vertexShader == 0) {
            return 0;
        }

        fragmentShader = loadGLShader(GLES30.GL_FRAGMENT_SHADER, fragment_resId);

        if (fragmentShader == 0) {
            return 0;
        }

        programObject = GLES30.glCreateProgram();

        if (programObject == 0) {
            Log.e(TAG_TH, "Error creating program");
            return 0;
        }

        GLES30.glAttachShader(programObject, vertexShader);
        GLES30.glAttachShader(programObject, fragmentShader);

        // Link the program
        GLES30.glLinkProgram(programObject);

        // Check the link status
        GLES30.glGetProgramiv(programObject, GLES30.GL_LINK_STATUS, linked, 0);

        if (linked[0] == 0) {
            Log.e(TAG_TH, "Error linking program.");
            Log.e(TAG_TH, GLES30.glGetProgramInfoLog(programObject));
            GLES30.glDeleteProgram(programObject);
            return 0;
        }

        // Free up no longer needed shader resources
        GLES30.glDeleteShader(vertexShader);
        GLES30.glDeleteShader(fragmentShader);

//        Log.d(TAG_TH, DBG_LC +" loadProgram END");
        return programObject;
    }

    protected void updateModelPosition() {
//        Log.d(TAG_TH, DBG_LC + "updateModelPosition BEGIN");
        Matrix.setIdentityM(modelCube, 0);
        Matrix.translateM(modelCube, 0, modelPosition[0], modelPosition[1], modelPosition[2]);
        checkGLError("updateCubePosition");
//        Log.d(TAG_TH, DBG_LC + "updateModelPosition END");
    }

    private int loadGLShader(int type, int resId) {
//        Log.d(TAG_TH, DBG_LC +" loadGLShader BEGIN");
        String code = readRawTextFile(resId);
        int shader = GLES30.glCreateShader(type);
        GLES30.glShaderSource(shader, code);
        GLES30.glCompileShader(shader);

        // Get the copilation status.
        final int[] compileStatus = new int[1];
        GLES30.glGetShaderiv(shader, GLES30.GL_COMPILE_STATUS, compileStatus, 0);

        // If the compilation failed, delete the shader.
        if (compileStatus[0] == 0) {
            Log.e(TAG_TH, "Error compiling shader: " + GLES30.glGetShaderInfoLog(shader));
            GLES30.glDeleteShader(shader);
            shader = 0;
        }

        if (shader == 0) {
            throw new RuntimeException("Error creating shader.");
        }
//        Log.d(TAG_TH, DBG_LC + " loadGLShader END");
        return shader;
    }

    private String readRawTextFile(int resId) {
//        Log.d(TAG_TH, DBG_LC +" readRawTextFile BEGIN");
        InputStream inputStream = getResources().openRawResource(resId);
        try {
            BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append("\n");
            }
            reader.close();
//            Log.d(TAG_TH, DBG_LC + " readRawTextFile END");
            return sb.toString();
        } catch (IOException e) {
            e.printStackTrace();
        }
//        Log.d(TAG_TH, DBG_LC +" readRawTextFile END");
        return null;
    }

    public void getPerspective(float near, float far, float left, float right, float bottom, float top, float[] perspective, int offset) {
//        Log.d(TAG_TH, DBG_LC + "getPerspective BEGIN");
        float l = (float) (-Math.tan(Math.toRadians((double) left))) * near;
        float r = (float) Math.tan(Math.toRadians((double) right)) * near;
        float b = (float) (-Math.tan(Math.toRadians((double) bottom))) * near;
        float t = (float) Math.tan(Math.toRadians((double) top)) * near;
        Matrix.frustumM(perspective, offset, l, r, b, t, near, far);
//        Log.d(TAG_TH, DBG_LC + "getPerspective END");
    }

    public void drawCube() {
//        Log.d(TAG_TH, DBG_LC + "drawCube BEGIN");
        GLES30.glUseProgram(cubeProgram);
        GLES30.glUniform3fv(cubeLightPosParam, 1, lightPosInEyeSpace, 0);

        // Set the Model in the shader, used to calculate lighting
        GLES30.glUniformMatrix4fv(cubeModelParam, 1, false, modelCube, 0);

        // Set the ModelView in the shader, used to calculate lighting
        GLES30.glUniformMatrix4fv(cubeModelViewParam, 1, false, modelView, 0);

        GLES30.glUniformMatrix4fv(cubeModelViewProjectionParam, 1, false, modelViewProjection, 0);
        // Bind the VAO
        if (isLookingAtObject()) {
            GLES30.glBindVertexArray(cubeFoundVAOId[0]);
        } else {
            GLES30.glBindVertexArray(cubeGeneralVAOId[0]);
        }


        // Draw with the VAO settings
        GLES30.glDrawElements(GLES30.GL_TRIANGLES, WorldLayoutData.CUBE_INDEX.length, GLES30.GL_UNSIGNED_SHORT, 0);

        // Return to the default VAO
        GLES30.glBindVertexArray(0);
//        Log.d(TAG_TH, DBG_LC + "drawCube END");
    }

    public void drawFloor() {
//        Log.d(TAG_TH, DBG_LC + "drawFloor BEGIN");
        GLES30.glUseProgram(floorProgram);
        GLES30.glUniform3fv(floorLightPosParam, 1, lightPosInEyeSpace, 0);
        GLES30.glUniformMatrix4fv(floorModelParam, 1, false, modelFloor, 0);
        GLES30.glUniformMatrix4fv(floorModelViewParam, 1, false, modelView, 0);
        GLES30.glUniformMatrix4fv(floorModelViewProjectionParam, 1, false, modelViewProjection, 0);
        GLES30.glBindVertexArray(floorVAOId[0]);
        GLES30.glDrawElements(GLES30.GL_TRIANGLES, WorldLayoutData.FLOOR_INDEX.length, GLES30.GL_UNSIGNED_SHORT, 0);
        GLES30.glBindVertexArray(0);
        checkGLError("drawing floor");
//        Log.d(TAG_TH, DBG_LC + "drawFloor END");
    }

    private boolean isLookingAtObject() {
//        Log.d(TAG_TH, DBG_LC + "isLookingAtObject BEGIN");
        // Convert object space to camera space. Use the headView from onNewFrame.
        Matrix.multiplyMM(modelView, 0, headView, 0, modelCube, 0);
        Matrix.multiplyMV(tempPosition, 0, modelView, 0, POS_MATRIX_MULTIPLY_VEC, 0);

        float pitch = (float) Math.atan2(tempPosition[1], -tempPosition[2]);
        float yaw = (float) Math.atan2(tempPosition[0], -tempPosition[2]);

//        Log.d(TAG_TH, DBG_LC + "isLookingAtObject END");
        return Math.abs(pitch) < PITCH_LIMIT && Math.abs(yaw) < YAW_LIMIT;
    }

    protected void setCubeRotation() {
        Matrix.rotateM(modelCube, 0, TIME_DELTA, 0.5f, 0.5f, 1.0f);
    }

    public void quternion2Matrix(float Q[], float M[]) {
        float x = Q[0];
        float y = Q[1];
        float z = Q[2];
        float w = Q[3];
        float ww = w * w;
        float xx = x * x;
        float yy = y * y;
        float zz = z * z;

        M[0] = ww + xx - yy - zz;
        M[1] = 2 * (x * y - w * z);
        M[2] = 2 * (x * z + w * y);
        M[3] = 0.f;


        M[4] = 2 * (x * y + w * z);
        M[5] = ww - xx + yy - zz;
        M[6] = 2 * (y * z - w * x);
        M[7] = 0.f;


        M[8] = 2 * (x * z - w * y);
        ;
        M[9] = 2 * (y * z + w * x);
        M[10] = ww - xx - yy + zz;
        M[11] = 0.f;


        M[12] = 0.0f;
        M[13] = 0.0f;
        M[14] = 0.0f;
        M[15] = 1.f;
    }

    private static void checkGLError(String label) {
//        Log.d(TAG_TH, DBG_LC + " checkGLError BEGIN");
        int error;
        while ((error = GLES30.glGetError()) != GLES30.GL_NO_ERROR) {
            Log.e(TAG_TH, label + ": glError " + error);
            throw new RuntimeException(label + ": glError " + error);
        }
//        Log.d(TAG_TH, DBG_LC + " checkGLError END");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
//        Log.d(TAG_TH, DBG_LC + "onCreate BEGIN");
        getWindow().setFlags(
                WindowManager.LayoutParams.FLAG_FULLSCREEN
                        | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                WindowManager.LayoutParams.FLAG_FULLSCREEN
                        | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        super.onCreate(savedInstanceState);

        modelBtn = new float[16];
        modelCube = new float[16];
        modelCubeController = new float[16];
        camera = new float[16];
        view = new float[16];
        modelViewProjection = new float[16];
        modelView = new float[16];
        modelFloor = new float[16];
        tempPosition = new float[4];
        modelPosition = new float[]{0.0f, 0.0f, -MAX_MODEL_DISTANCE / 2.0f};
        headView = new float[16];
        headOritation = new float[4];

        cube_General_Vertics = ByteBuffer.allocateDirect(WorldLayoutData.CUBE_GENERAL.length * 4)
                .order(ByteOrder.nativeOrder()).asFloatBuffer();
        cube_General_Vertics.put(WorldLayoutData.CUBE_GENERAL).position(0);

        cube_Found_Vertics = ByteBuffer.allocateDirect(WorldLayoutData.CUBE_FOUND.length * 4)
                .order(ByteOrder.nativeOrder()).asFloatBuffer();
        cube_Found_Vertics.put(WorldLayoutData.CUBE_FOUND).position(0);

        cube_Indexs = ByteBuffer.allocateDirect(WorldLayoutData.CUBE_INDEX.length * 2)
                .order(ByteOrder.nativeOrder()).asShortBuffer();
        cube_Indexs.put(WorldLayoutData.CUBE_INDEX).position(0);

        floor_Vertics = ByteBuffer.allocateDirect(WorldLayoutData.FLOOR.length * 4)
                .order(ByteOrder.nativeOrder()).asFloatBuffer();
        floor_Vertics.put(WorldLayoutData.FLOOR).position(0);

        floor_Indexs = ByteBuffer.allocateDirect(WorldLayoutData.FLOOR_INDEX.length * 2)
                .order(ByteOrder.nativeOrder()).asShortBuffer();
        floor_Indexs.put(WorldLayoutData.FLOOR_INDEX).position(0);

//        Log.d(TAG_TH, DBG_LC + "onCreate END");
    }

    @Override
    protected void onPause() {
        super.RenderEventPush(UER_EVENT + 1);
        super.onPause();
//        Log.d(TAG_TH, DBG_LC + "onPause CALLED");
    }

    @Override
    protected void onResume() {
        super.onResume();
        mTime0 = System.currentTimeMillis();    // Add by Enoch : FPS relative.
        mFPS = 0;    // Add by Enoch : FPS relative.
//        Log.d(TAG_TH, DBG_LC + "onResume CALLED");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
//        Log.d(TAG_TH, DBG_LC + "onDestroy CALLED");
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        Log.d(TAG_TH, DBG_LC + " onKeyDown:" + keyCode);

        if (keyCode == 96) {
            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onStart() {
        super.onStart();
//        Log.d(TAG_TH, DBG_LC + "onStart CALLED");
    }

    @Override
    protected void onRestart() {
        super.onRestart();
//        Log.d(TAG_TH, DBG_LC + "onRestart CALLED");
    }

    @Override
    protected void onStop() {
        super.onStop();
//        Log.d(TAG_TH, DBG_LC + "onStop CALLED");
    }
}
