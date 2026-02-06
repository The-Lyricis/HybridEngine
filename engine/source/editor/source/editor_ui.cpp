#include"editor_ui.h"
#include <imgui.h>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace Hybrid {
    void EditorUI::initialize(GLFWwindow* window) {
        m_window = window; // 由外部传入

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // 注意：第二个参数传 true，代表让 ImGui 自动安装它的回调
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void EditorUI::display() {
        if (!m_window) return;
        
        ImGui_ImplOpenGL3_NewFrame(); //通知opengl开始新的一帧
        ImGui_ImplGlfw_NewFrame(); //通知glfw开始新的一帧
        ImGui::NewFrame();//通知imgui开始新的一帧
        
        //以下是UI代码
        ImGui::Begin("SaluteChickEngine"); //创建一个新窗口
        ImGui::Text("Hello, engine"); //在窗口中显示文本
        ImGui::End();//结束窗口定义
        
        ImGui::Render();//渲染数据
        
        int display_w = 0, display_h = 0;//定义宽高变量
        glfwGetFramebufferSize(m_window, &display_w, &display_h);//获取窗口对应 framebuffer 的实际像素大小
        glViewport(0, 0, display_w, display_h);//设置视口大小
        glClear(GL_COLOR_BUFFER_BIT);//清除颜色缓冲区
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        //将 ImGui::Render() 生成的绘制数据交给 OpenGL3 后端进行实际渲染
        //GetDrawData() 返回本帧 UI 的 draw lists 
        
        glfwSwapBuffers(m_window);//交换前后缓冲区
    }
}

//namespace Hybrid
//{
//	void EditorUI::initialize()
//	{
//		if (!glfwInit())return; //glfw初始化
//		window = glfwCreateWindow(800, 600, "ImGui Window", nullptr, nullptr); //创建一个glfw窗口
//		glfwMakeContextCurrent(window); //设置当前线程上下文
//		glfwSwapInterval(1); //设置交换间隔(启用垂直同步)
//
//		IMGUI_CHECKVERSION(); //检查imgui版本
//		ImGui::CreateContext(); //创建imgui上下文
//
//		if (!ImGui_ImplGlfw_InitForOpenGL(window, true))return; //初始化imgui的glfw后端
//		ImGui_ImplOpenGL3_Init("#version 330"); //初始化imgui的opengl3后端
//	}
//	void EditorUI::display()
//	{
//		if (!window) return;
//
//		glfwPollEvents(); //轮询窗口事件
//
//		ImGui_ImplOpenGL3_NewFrame(); //通知opengl开始新的一帧
//		ImGui_ImplGlfw_NewFrame(); //通知glfw开始新的一帧
//		ImGui::NewFrame();//通知imgui开始新的一帧
//
//		//以下是UI代码
//		ImGui::Begin("SaluteChickEngine"); //创建一个新窗口
//		ImGui::Text("Hello, engine"); //在窗口中显示文本
//		ImGui::End();//结束窗口定义
//
//		ImGui::Render();//渲染数据
//
//		int display_w = 0, display_h = 0;//定义宽高变量
//		glfwGetFramebufferSize(window, &display_w, &display_h);//获取窗口对应 framebuffer 的实际像素大小
//		glViewport(0, 0, display_w, display_h);//设置视口大小
//		glClear(GL_COLOR_BUFFER_BIT);//清除颜色缓冲区
//
//		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
//		//将 ImGui::Render() 生成的绘制数据交给 OpenGL3 后端进行实际渲染
//		//GetDrawData() 返回本帧 UI 的 draw lists 
//
//		glfwSwapBuffers(window);//交换前后缓冲区
//	}
//	bool EditorUI::isWindowShouldClose()
//	{
//		return glfwWindowShouldClose(window);//检查窗口是否应该关闭
//	}
//	void EditorUI::cleanup()
//	{
//		ImGui_ImplOpenGL3_Shutdown();//关闭opengl3后端
//		ImGui_ImplGlfw_Shutdown();//关闭glfw后端
//		ImGui::DestroyContext();//销毁imgui上下文
//		glfwDestroyWindow(window);//销毁窗口
//		glfwTerminate();//终止glfw
//	}
//}