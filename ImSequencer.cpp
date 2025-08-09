// https://github.com/CedricGuillemet/ImGuizmo
// v 1.89 WIP
//
// The MIT License(MIT)
//
// Copyright(c) 2021 Cedric Guillemet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include "ImSequencer.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdlib>

namespace ImSequencer
{
#ifndef IMGUI_DEFINE_MATH_OPERATORS
   static ImVec2 operator+(const ImVec2& a, const ImVec2& b) {
      return ImVec2(a.x + b.x, a.y + b.y);
   }
#endif
   static bool SequencerAddDelButton(ImDrawList* draw_list, ImVec2 pos, bool add = true)
   {
      ImGuiIO& io = ImGui::GetIO();
      ImRect btnRect(pos, ImVec2(pos.x + 16, pos.y + 16));
      bool overBtn = btnRect.Contains(io.MousePos);
      bool containedClick = overBtn && btnRect.Contains(io.MouseClickedPos[0]);
      bool clickedBtn = containedClick && io.MouseReleased[0];
      int btnColor = overBtn ? 0xAAEAFFAA : 0x77A3B2AA;
      if (containedClick && io.MouseDownDuration[0] > 0)
         btnRect.Expand(2.0f);

      float midy = pos.y + 16 / 2 - 0.5f;
      float midx = pos.x + 16 / 2 - 0.5f;
      draw_list->AddRect(btnRect.Min, btnRect.Max, btnColor, 4);
      draw_list->AddLine(ImVec2(btnRect.Min.x + 3, midy), ImVec2(btnRect.Max.x - 3, midy), btnColor, 2);
      if (add)
         draw_list->AddLine(ImVec2(midx, btnRect.Min.y + 3), ImVec2(midx, btnRect.Max.y - 3), btnColor, 2);
      return clickedBtn;
   }

   bool Sequencer(SequencerState* state, SequenceInterface* sequence, int* currentFrame, bool* expanded, int* selectedEntry, int* firstFrame, int sequenceOptions)
   {
      auto& style = ImGui::GetStyle();

      bool ret = false;
      ImGuiIO& io = ImGui::GetIO();
      int cx = (int)(io.MousePos.x);
      int cy = (int)(io.MousePos.y);
      int legendWidth = 200;

      int delEntry = -1;
      int dupEntry = -1;
      int ItemHeight = 20;

      bool popupOpened = false;
      int sequenceCount = sequence->GetItemCount();
      if (!sequenceCount)
         return false;
      ImGui::BeginGroup();

      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 canvas_pos = ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!
      ImVec2 canvas_size = ImGui::GetContentRegionAvail();        // Resize canvas to what's available
      int firstFrameUsed = firstFrame ? *firstFrame : 0;


      int controlHeight = sequenceCount * ItemHeight;
      for (int i = 0; i < sequenceCount; i++)
         controlHeight += int(sequence->GetCustomHeight(i));
      int frameCount = ImMax(sequence->GetFrameMax() - sequence->GetFrameMin(), 1);

      struct CustomDraw
      {
         int index;
         ImRect customRect;
         ImRect legendRect;
         ImRect clippingRect;
         ImRect legendClippingRect;
      };
      ImVector<CustomDraw> customDraws;
      ImVector<CustomDraw> compactCustomDraws;
      // zoom in/out
      const int visibleFrameCount = (int)floorf((canvas_size.x - legendWidth) / state->framePixelWidth);
      const float barWidthRatio = ImMin(visibleFrameCount / (float)frameCount, 1.f);
      const float barWidthInPixels = barWidthRatio * (canvas_size.x - legendWidth);

      ImRect regionRect(canvas_pos, canvas_pos + canvas_size);

      if (ImGui::IsWindowFocused() && io.KeyAlt && io.MouseDown[2])
      {
         if (!state->panningView)
         {
            state->panningViewSource = io.MousePos;
            state->panningView = true;
            state->panningViewFrame = *firstFrame;
         }
         *firstFrame = state->panningViewFrame - int((io.MousePos.x - state->panningViewSource.x) / state->framePixelWidth);
         *firstFrame = ImClamp(*firstFrame, sequence->GetFrameMin(), sequence->GetFrameMax() - visibleFrameCount);
      }
      if (state->panningView && !io.MouseDown[2])
      {
         state->panningView = false;
      }
      state->framePixelWidthTarget = ImClamp(state->framePixelWidthTarget, 0.1f, 50.f);

      state->framePixelWidth = ImLerp(state->framePixelWidth, state->framePixelWidthTarget, 0.33f);

      frameCount = sequence->GetFrameMax() - sequence->GetFrameMin();
      if (visibleFrameCount >= frameCount && firstFrame)
         *firstFrame = sequence->GetFrameMin();


      // --
      if (expanded && !*expanded)
      {
         ImGui::InvisibleButton("canvas", ImVec2(canvas_size.x - canvas_pos.x, (float)ItemHeight));
         draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_size.x + canvas_pos.x, canvas_pos.y + ItemHeight), 0xFF3D3837, 0);
         char tmps[512];
         ImFormatString(tmps, IM_ARRAYSIZE(tmps), sequence->GetCollapseFmt(), frameCount, sequenceCount);
         draw_list->AddText(ImVec2(canvas_pos.x + 26, canvas_pos.y + 2), 0xFFFFFFFF, tmps);
      }
      else
      {
         bool hasScrollBar(true);
         /*
         int framesPixelWidth = int(frameCount * state->framePixelWidth);
         if ((framesPixelWidth + legendWidth) >= canvas_size.x)
         {
             hasScrollBar = true;
         }
         */
         // test scroll area
         ImVec2 headerSize(canvas_size.x, (float)ItemHeight);
         ImVec2 scrollBarSize(canvas_size.x, 14.f);
         ImGui::InvisibleButton("topBar", headerSize);
         draw_list->AddRectFilled(canvas_pos, canvas_pos + headerSize, 0xFFFF0000, 0);
         ImVec2 childFramePos = ImGui::GetCursorScreenPos();
         ImVec2 childFrameSize(canvas_size.x, canvas_size.y - 8.f - headerSize.y - (hasScrollBar ? scrollBarSize.y : 0));
         ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
         ImGui::BeginChildFrame(889, childFrameSize);
         sequence->focused = ImGui::IsWindowFocused();
         ImGui::InvisibleButton("contentBar", ImVec2(canvas_size.x, float(controlHeight)));
         const ImVec2 contentMin = ImGui::GetItemRectMin();
         const ImVec2 contentMax = ImGui::GetItemRectMax();
         const ImRect contentRect(contentMin, contentMax);
         const float contentHeight = contentMax.y - contentMin.y;

         // full background
         draw_list->AddRectFilled(canvas_pos, canvas_pos + canvas_size, ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_ChildBg]), 0);

         // current frame top
         ImRect topRect(ImVec2(canvas_pos.x + legendWidth, canvas_pos.y), ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + ItemHeight));

         if (!state->MovingCurrentFrame && !state->MovingScrollBar && state->movingEntry == -1 && sequenceOptions & SEQUENCER_CHANGE_FRAME && currentFrame && *currentFrame >= 0 && topRect.Contains(io.MousePos) && io.MouseDown[0])
         {
            state->MovingCurrentFrame = true;
         }
         if (state->MovingCurrentFrame)
         {
            if (frameCount)
            {
               *currentFrame = (int)((io.MousePos.x - topRect.Min.x) / state->framePixelWidth) + firstFrameUsed;
               if (*currentFrame < sequence->GetFrameMin())
                  *currentFrame = sequence->GetFrameMin();
               if (*currentFrame >= sequence->GetFrameMax())
                  *currentFrame = sequence->GetFrameMax();
            }
            if (!io.MouseDown[0])
               state->MovingCurrentFrame = false;
         }

         //header
         draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_size.x + canvas_pos.x, canvas_pos.y + ItemHeight), 0xFF3D3837, 0);
         if (sequenceOptions & SEQUENCER_ADD)
         {
            if (SequencerAddDelButton(draw_list, ImVec2(canvas_pos.x + legendWidth - ItemHeight, canvas_pos.y + 2), true))
               ImGui::OpenPopup("addEntry");

            if (ImGui::BeginPopup("addEntry"))
            {
               for (int i = 0; i < sequence->GetItemTypeCount(); i++)
                  if (ImGui::Selectable(sequence->GetItemTypeName(i)))
                  {
                     sequence->Add(i);
                     *selectedEntry = sequence->GetItemCount() - 1;
                  }

               ImGui::EndPopup();
               popupOpened = true;
            }
         }

         //header frame number and lines
         int modFrameCount = 10;
         int frameStep = 1;
         while ((modFrameCount * state->framePixelWidth) < 150)
         {
            modFrameCount *= 2;
            frameStep *= 2;
         };
         int halfModFrameCount = modFrameCount / 2;

         auto drawLine = [&](int i, int regionHeight) {
            bool baseIndex = ((i % modFrameCount) == 0) || (i == sequence->GetFrameMax() || i == sequence->GetFrameMin());
            bool halfIndex = (i % halfModFrameCount) == 0;
            int px = (int)canvas_pos.x + int(i * state->framePixelWidth) + legendWidth - int(firstFrameUsed * state->framePixelWidth);
            int tiretStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
            int tiretEnd = baseIndex ? regionHeight : ItemHeight;

            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legendWidth))
            {
               draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)tiretStart), ImVec2((float)px, canvas_pos.y + (float)tiretEnd - 1), 0xFF606060, 1);

               draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)ItemHeight), ImVec2((float)px, canvas_pos.y + (float)regionHeight - 1), 0x30606060, 1);
            }

            if (baseIndex && px > (canvas_pos.x + legendWidth))
            {
               char tmps[512];
               ImFormatString(tmps, IM_ARRAYSIZE(tmps), "%d", i);
               draw_list->AddText(ImVec2((float)px + 3.f, canvas_pos.y), 0xFFBBBBBB, tmps);
            }

         };

         auto drawLineContent = [&](int i, int /*regionHeight*/) {
            int px = (int)canvas_pos.x + int(i * state->framePixelWidth) + legendWidth - int(firstFrameUsed * state->framePixelWidth);
            int tiretStart = int(contentMin.y);
            int tiretEnd = int(contentMax.y);

            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legendWidth))
            {
               //draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)tiretStart), ImVec2((float)px, canvas_pos.y + (float)tiretEnd - 1), 0xFF606060, 1);

               draw_list->AddLine(ImVec2(float(px), float(tiretStart)), ImVec2(float(px), float(tiretEnd)), 0x30606060, 1);
            }
         };
         for (int i = sequence->GetFrameMin(); i <= sequence->GetFrameMax(); i += frameStep)
         {
            drawLine(i, ItemHeight);
         }
         drawLine(sequence->GetFrameMin(), ItemHeight);
         drawLine(sequence->GetFrameMax(), ItemHeight);
         /*
                  draw_list->AddLine(canvas_pos, ImVec2(canvas_pos.x, canvas_pos.y + controlHeight), 0xFF000000, 1);
                  draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + ItemHeight), ImVec2(canvas_size.x, canvas_pos.y + ItemHeight), 0xFF000000, 1);
                  */
                  // clip content

         draw_list->PushClipRect(childFramePos, childFramePos + childFrameSize, true);

         // draw item names in the legend rect on the left
         size_t customHeight = 0;
         for (int i = 0; i < sequenceCount; i++)
         {
            int type;
            sequence->Get(i, NULL, NULL, &type, NULL);
            ImVec2 tpos(contentMin.x + 3, contentMin.y + i * ItemHeight + 2 + customHeight);
            draw_list->AddText(tpos, 0xFFFFFFFF, sequence->GetItemLabel(i));

            if (sequenceOptions & SEQUENCER_DEL)
            {
               if (SequencerAddDelButton(draw_list, ImVec2(contentMin.x + legendWidth - ItemHeight + 2 - 10, tpos.y + 2), false))
                  delEntry = i;

               if (SequencerAddDelButton(draw_list, ImVec2(contentMin.x + legendWidth - ItemHeight - ItemHeight + 2 - 10, tpos.y + 2), true))
                  dupEntry = i;
            }
            customHeight += sequence->GetCustomHeight(i);
         }

         // slots background
         customHeight = 0;
         for (int i = 0; i < sequenceCount; i++)
         {
            unsigned int col = (i & 1) ? 0xFF3A3636 : 0xFF413D3D;

            size_t localCustomHeight = sequence->GetCustomHeight(i);
            ImVec2 pos = ImVec2(contentMin.x + legendWidth, contentMin.y + ItemHeight * i + 1 + customHeight);
            ImVec2 sz = ImVec2(canvas_size.x + canvas_pos.x, pos.y + ItemHeight - 1 + localCustomHeight);
            if (!popupOpened && cy >= pos.y && cy < pos.y + (ItemHeight + localCustomHeight) && state->movingEntry == -1 && cx>contentMin.x && cx < contentMin.x + canvas_size.x)
            {
               col += 0x80201008;
               pos.x -= legendWidth;
            }
            draw_list->AddRectFilled(pos, sz, col, 0);
            customHeight += localCustomHeight;
         }

         draw_list->PushClipRect(childFramePos + ImVec2(float(legendWidth), 0.f), childFramePos + childFrameSize, true);

         // vertical frame lines in content area
         for (int i = sequence->GetFrameMin(); i <= sequence->GetFrameMax(); i += frameStep)
         {
            drawLineContent(i, int(contentHeight));
         }
         drawLineContent(sequence->GetFrameMin(), int(contentHeight));
         drawLineContent(sequence->GetFrameMax(), int(contentHeight));

         // selection
         bool selected = selectedEntry && (*selectedEntry >= 0);
         if (selected)
         {
            customHeight = 0;
            for (int i = 0; i < *selectedEntry; i++)
               customHeight += sequence->GetCustomHeight(i);
            draw_list->AddRectFilled(ImVec2(contentMin.x, contentMin.y + ItemHeight * *selectedEntry + customHeight), ImVec2(contentMin.x + canvas_size.x, contentMin.y + ItemHeight * (*selectedEntry + 1) + customHeight), 0x801080FF, 1.f);
         }

         // slots
         customHeight = 0;
         ImRect backgroundRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y));
         for (int i = 0; i < sequenceCount; i++)
         {
            int* start, * end;
            unsigned int color;
            sequence->Get(i, &start, &end, NULL, &color);
            size_t localCustomHeight = sequence->GetCustomHeight(i);

            if (sequenceOptions & SEQUENCER_DRAW_SUB_RANGE)
            {
               ImVec2 pos = ImVec2(contentMin.x + legendWidth - firstFrameUsed * state->framePixelWidth, contentMin.y + ItemHeight * i + 1 + customHeight);
               ImVec2 slotP1(pos.x + *start * state->framePixelWidth, pos.y + 2);
               ImVec2 slotP2(pos.x + *end * state->framePixelWidth + state->framePixelWidth, pos.y + ItemHeight - 2);
               ImVec2 slotP3(pos.x + *end * state->framePixelWidth + state->framePixelWidth, pos.y + ItemHeight - 2 + localCustomHeight);
               unsigned int slotColor = color;// | 0xFF000000;
               unsigned int slotColorHalf = (color & 0xFFFFFF) | 0x20000000;

               if (slotP1.x <= (canvas_size.x + contentMin.x) && slotP2.x >= (contentMin.x + legendWidth))
               {
                  draw_list->AddRectFilled(slotP1, slotP3, slotColorHalf, 2);
                  draw_list->AddRectFilled(slotP1, slotP2, slotColor, 2);
               }
               if (ImRect(slotP1, slotP2).Contains(io.MousePos) && io.MouseDoubleClicked[0])
               {
                  sequence->DoubleClick(i);
               }
               // Ensure grabbable handles
               const float max_handle_width = slotP2.x - slotP1.x / 3.0f;
               const float min_handle_width = ImMin(10.0f, max_handle_width);
               const float handle_width = ImClamp(state->framePixelWidth / 2.0f, min_handle_width, max_handle_width);
               ImRect rects[3] = { ImRect(slotP1, ImVec2(slotP1.x + handle_width, slotP2.y))
                  , ImRect(ImVec2(slotP2.x - handle_width, slotP1.y), slotP2)
                  , ImRect(slotP1, slotP2) };

               const unsigned int quadColor[] = { 0xFFFFFFFF, 0xFFFFFFFF, slotColor + (selected ? 0 : 0x202020) };
               if (state->movingEntry == -1 && (sequenceOptions & SEQUENCER_EDIT_STARTEND) && backgroundRect.Contains(io.MousePos))
               {
                  for (int j = 2; j >= 0; j--)
                  {
                     ImRect& rc = rects[j];
                     if (!rc.Contains(io.MousePos))
                        continue;
                     draw_list->AddRectFilled(rc.Min, rc.Max, quadColor[j], 2);
                  }

                  for (int j = 0; j < 3; j++)
                  {
                     ImRect& rc = rects[j];
                     if (!rc.Contains(io.MousePos))
                        continue;
                     if (!ImRect(childFramePos, childFramePos + childFrameSize).Contains(io.MousePos))
                        continue;
                     if (ImGui::IsMouseClicked(0) && !state->MovingScrollBar && !state->MovingCurrentFrame)
                     {
                        state->movingEntry = i;
                        state->movingPos = cx;
                        state->movingPart = j + 1;
                        sequence->BeginEdit(state->movingEntry);
                        break;
                     }
                  }
               }
            }

            // custom draw
            if (localCustomHeight > 0)
            {
               ImVec2 rp(canvas_pos.x, contentMin.y + ItemHeight * i + 1 + customHeight);
               ImRect customRect(rp + ImVec2(legendWidth - (firstFrameUsed - sequence->GetFrameMin() - 0.5f) * state->framePixelWidth, float(ItemHeight)),
                  rp + ImVec2(legendWidth + (sequence->GetFrameMax() - firstFrameUsed - 0.5f + 2.f) * state->framePixelWidth, float(localCustomHeight + ItemHeight)));
               ImRect clippingRect(rp + ImVec2(float(legendWidth), float(ItemHeight)), rp + ImVec2(canvas_size.x, float(localCustomHeight + ItemHeight)));

               ImRect legendRect(rp + ImVec2(0.f, float(ItemHeight)), rp + ImVec2(float(legendWidth), float(localCustomHeight)));
               ImRect legendClippingRect(canvas_pos + ImVec2(0.f, float(ItemHeight)), canvas_pos + ImVec2(float(legendWidth), float(localCustomHeight + ItemHeight)));
               customDraws.push_back({ i, customRect, legendRect, clippingRect, legendClippingRect });
            }
            else
            {
               ImVec2 rp(canvas_pos.x, contentMin.y + ItemHeight * i + customHeight);
               ImRect customRect(rp + ImVec2(legendWidth - (firstFrameUsed - sequence->GetFrameMin() - 0.5f) * state->framePixelWidth, float(0.f)),
                  rp + ImVec2(legendWidth + (sequence->GetFrameMax() - firstFrameUsed - 0.5f + 2.f) * state->framePixelWidth, float(ItemHeight)));
               ImRect clippingRect(rp + ImVec2(float(legendWidth), float(0.f)), rp + ImVec2(canvas_size.x, float(ItemHeight)));

               compactCustomDraws.push_back({ i, customRect, ImRect(), clippingRect, ImRect() });
            }
            customHeight += localCustomHeight;
         }


         // moving
         if (backgroundRect.Contains(io.MousePos) && state->movingEntry >= 0)
         {
#if IMGUI_VERSION_NUM >= 18723
            ImGui::SetNextFrameWantCaptureMouse(true);
#else
            ImGui::CaptureMouseFromApp();
#endif
            int diffFrame = int((cx - state->movingPos) / state->framePixelWidth);
            if (std::abs(diffFrame) > 0)
            {
               int* start, * end;
               sequence->Get(state->movingEntry, &start, &end, NULL, NULL);
               if (selectedEntry)
                  *selectedEntry = state->movingEntry;
               int& l = *start;
               int& r = *end;
               if (state->movingPart & 1)
                  l += diffFrame;
               if (state->movingPart & 2)
                  r += diffFrame;
               if (l < 0)
               {
                  if (state->movingPart & 2)
                     r -= l;
                  l = 0;
               }
               if (state->movingPart & 1 && l > r)
                  l = r;
               if (state->movingPart & 2 && r < l)
                  r = l;
               state->movingPos += int(diffFrame * state->framePixelWidth);
            }
            if (!io.MouseDown[0])
            {
               // single select
               if (!diffFrame && state->movingPart && selectedEntry)
               {
                  *selectedEntry = state->movingEntry;
                  ret = true;
               }

               state->movingEntry = -1;
               sequence->EndEdit();
            }
         }

         // cursor
         if (currentFrame && firstFrame && *currentFrame >= *firstFrame && *currentFrame <= sequence->GetFrameMax())
         {
            float cursorOffset = contentMin.x + legendWidth + (*currentFrame - firstFrameUsed) * state->framePixelWidth + state->framePixelWidth / 2 - state->cursorWidth * 0.5f;
            draw_list->AddLine(ImVec2(cursorOffset, canvas_pos.y), ImVec2(cursorOffset, contentMax.y), ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_PlotHistogram]), state->cursorWidth);
            char tmps[512];
            ImFormatString(tmps, IM_ARRAYSIZE(tmps), "%d", *currentFrame);
            draw_list->AddText(ImVec2(cursorOffset + 10, canvas_pos.y + 2), 0xFF2A2AFF, tmps);
         }

         draw_list->PopClipRect();
         draw_list->PopClipRect();

         for (auto& customDraw : customDraws)
            sequence->CustomDraw(customDraw.index, draw_list, customDraw.customRect, customDraw.legendRect, customDraw.clippingRect, customDraw.legendClippingRect);
         for (auto& customDraw : compactCustomDraws)
            sequence->CustomDrawCompact(customDraw.index, draw_list, customDraw.customRect, customDraw.clippingRect);

         // copy paste
         if (sequenceOptions & SEQUENCER_COPYPASTE)
         {
            ImRect rectCopy(ImVec2(contentMin.x + 100, canvas_pos.y + 2)
               , ImVec2(contentMin.x + 100 + 30, canvas_pos.y + ItemHeight - 2));
            bool inRectCopy = rectCopy.Contains(io.MousePos);
            unsigned int copyColor = inRectCopy ? 0xFF1080FF : 0xFF000000;
            draw_list->AddText(rectCopy.Min, copyColor, "Copy");

            ImRect rectPaste(ImVec2(contentMin.x + 140, canvas_pos.y + 2)
               , ImVec2(contentMin.x + 140 + 30, canvas_pos.y + ItemHeight - 2));
            bool inRectPaste = rectPaste.Contains(io.MousePos);
            unsigned int pasteColor = inRectPaste ? 0xFF1080FF : 0xFF000000;
            draw_list->AddText(rectPaste.Min, pasteColor, "Paste");

            if (inRectCopy && io.MouseReleased[0])
            {
               sequence->Copy();
            }
            if (inRectPaste && io.MouseReleased[0])
            {
               sequence->Paste();
            }
         }
         //

         ImGui::EndChildFrame();
         ImGui::PopStyleColor();
         if (hasScrollBar)
         {
            ImGui::InvisibleButton("scrollBar", scrollBarSize);
            ImVec2 scrollBarMin = ImGui::GetItemRectMin();
            ImVec2 scrollBarMax = ImGui::GetItemRectMax();

            // ratio = number of frames visible in control / number to total frames

            float startFrameOffset = ((float)(firstFrameUsed - sequence->GetFrameMin()) / (float)frameCount) * (canvas_size.x - legendWidth);
            ImVec2 scrollBarA(scrollBarMin.x + legendWidth, scrollBarMin.y - 2);
            ImVec2 scrollBarB(scrollBarMin.x + canvas_size.x, scrollBarMax.y - 1);
            draw_list->AddRectFilled(scrollBarA, scrollBarB, 0xFF222222, 0);

            ImRect scrollBarRect(scrollBarA, scrollBarB);
            bool inScrollBar = scrollBarRect.Contains(io.MousePos);

            draw_list->AddRectFilled(scrollBarA, scrollBarB, 0xFF101010, 8);


            ImVec2 scrollBarC(scrollBarMin.x + legendWidth + startFrameOffset, scrollBarMin.y);
            ImVec2 scrollBarD(scrollBarMin.x + legendWidth + barWidthInPixels + startFrameOffset, scrollBarMax.y - 2);
            draw_list->AddRectFilled(scrollBarC, scrollBarD, (inScrollBar || state->MovingScrollBar) ? 0xFF606060 : 0xFF505050, 6);

            ImRect barHandleLeft(scrollBarC, ImVec2(scrollBarC.x + 14, scrollBarD.y));
            ImRect barHandleRight(ImVec2(scrollBarD.x - 14, scrollBarC.y), scrollBarD);

            bool onLeft = barHandleLeft.Contains(io.MousePos);
            bool onRight = barHandleRight.Contains(io.MousePos);


            draw_list->AddRectFilled(barHandleLeft.Min, barHandleLeft.Max, (onLeft || state->sizingLBar) ? 0xFFAAAAAA : 0xFF666666, 6);
            draw_list->AddRectFilled(barHandleRight.Min, barHandleRight.Max, (onRight || state->sizingRBar) ? 0xFFAAAAAA : 0xFF666666, 6);

            ImRect scrollBarThumb(scrollBarC, scrollBarD);
            if (state->sizingRBar)
            {
               if (!io.MouseDown[0])
               {
                  state->sizingRBar = false;
               }
               else
               {
                  float barNewWidth = ImMax(barWidthInPixels + io.MouseDelta.x, state->MinBarWidth);
                  float barRatio = barNewWidth / barWidthInPixels;
                  state->framePixelWidthTarget = state->framePixelWidth = state->framePixelWidth / barRatio;
                  int newVisibleFrameCount = int((canvas_size.x - legendWidth) / state->framePixelWidthTarget);
                  int lastFrame = *firstFrame + newVisibleFrameCount;
                  if (lastFrame > sequence->GetFrameMax())
                  {
                     state->framePixelWidthTarget = state->framePixelWidth = (canvas_size.x - legendWidth) / float(sequence->GetFrameMax() - *firstFrame);
                  }
               }
            }
            else if (state->sizingLBar)
            {
               if (!io.MouseDown[0])
               {
                  state->sizingLBar = false;
               }
               else
               {
                  if (fabsf(io.MouseDelta.x) > FLT_EPSILON)
                  {
                     float barNewWidth = ImMax(barWidthInPixels - io.MouseDelta.x, state->MinBarWidth);
                     float barRatio = barNewWidth / barWidthInPixels;
                     float previousFramePixelWidthTarget = state->framePixelWidthTarget;
                     state->framePixelWidthTarget = state->framePixelWidth = state->framePixelWidth / barRatio;
                     int newVisibleFrameCount = int(visibleFrameCount / barRatio);
                     int newFirstFrame = *firstFrame + newVisibleFrameCount - visibleFrameCount;
                     newFirstFrame = ImClamp(newFirstFrame, sequence->GetFrameMin(), ImMax(sequence->GetFrameMax() - visibleFrameCount, sequence->GetFrameMin()));
                     if (newFirstFrame == *firstFrame)
                     {
                        state->framePixelWidth = state->framePixelWidthTarget = previousFramePixelWidthTarget;
                     }
                     else
                     {
                        *firstFrame = newFirstFrame;
                     }
                  }
               }
            }
            else
            {
               if (state->MovingScrollBar)
               {
                  if (!io.MouseDown[0])
                  {
                     state->MovingScrollBar = false;
                  }
                  else
                  {
                     float framesPerPixelInBar = barWidthInPixels / (float)visibleFrameCount;
                     *firstFrame = int((io.MousePos.x - state->panningViewSource.x) / framesPerPixelInBar) - state->panningViewFrame;
                     *firstFrame = ImClamp(*firstFrame, sequence->GetFrameMin(), ImMax(sequence->GetFrameMax() - visibleFrameCount, sequence->GetFrameMin()));
                  }
               }
               else
               {
                  if (scrollBarThumb.Contains(io.MousePos) && ImGui::IsMouseClicked(0) && firstFrame && !state->MovingCurrentFrame && state->movingEntry == -1)
                  {
                     state->MovingScrollBar = true;
                     state->panningViewSource = io.MousePos;
                     state->panningViewFrame = -*firstFrame;
                  }
                  if (!state->sizingRBar && onRight && ImGui::IsMouseClicked(0))
                     state->sizingRBar = true;
                  if (!state->sizingLBar && onLeft && ImGui::IsMouseClicked(0))
                     state->sizingLBar = true;

               }
            }
         }
      }

      ImGui::EndGroup();

      if (regionRect.Contains(io.MousePos))
      {
         bool overCustomDraw = false;
         for (auto& custom : customDraws)
         {
            if (custom.customRect.Contains(io.MousePos))
            {
               overCustomDraw = true;
            }
         }
         if (overCustomDraw)
         {
         }
         else
         {
#if 0
            frameOverCursor = *firstFrame + (int)(visibleFrameCount * ((io.MousePos.x - (float)legendWidth - canvas_pos.x) / (canvas_size.x - legendWidth)));
            //frameOverCursor = max(min(*firstFrame - visibleFrameCount / 2, frameCount - visibleFrameCount), 0);

            /**firstFrame -= frameOverCursor;
            *firstFrame *= state->framePixelWidthTarget / state->framePixelWidth;
            *firstFrame += frameOverCursor;*/
            if (io.MouseWheel < -FLT_EPSILON)
            {
               *firstFrame -= frameOverCursor;
               *firstFrame = int(*firstFrame * 1.1f);
               state->framePixelWidthTarget *= 0.9f;
               *firstFrame += frameOverCursor;
            }

            if (io.MouseWheel > FLT_EPSILON)
            {
               *firstFrame -= frameOverCursor;
               *firstFrame = int(*firstFrame * 0.9f);
               state->framePixelWidthTarget *= 1.1f;
               *firstFrame += frameOverCursor;
            }
#endif
         }
      }

      if (expanded)
      {
         if (SequencerAddDelButton(draw_list, ImVec2(canvas_pos.x + 2, canvas_pos.y + 2), !*expanded))
            *expanded = !*expanded;
      }

      if (delEntry != -1)
      {
         sequence->Del(delEntry);
         if (selectedEntry && (*selectedEntry == delEntry || *selectedEntry >= sequence->GetItemCount()))
            *selectedEntry = -1;
      }

      if (dupEntry != -1)
      {
         sequence->Duplicate(dupEntry);
      }
      return ret;
   }
}
