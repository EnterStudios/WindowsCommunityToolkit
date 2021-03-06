﻿// ******************************************************************
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THE CODE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
// THE CODE OR THE USE OR OTHER DEALINGS IN THE CODE.
// ******************************************************************

using Newtonsoft.Json;

namespace Microsoft.Toolkit.Uwp.Services.Twitter
{
    /// <summary>
    /// Twitter Media
    /// </summary>
    public class TwitterMedia
    {
        /// <summary>
        /// Gets or sets MediaUrl.
        /// </summary>
        [JsonProperty("media_url")]
        public string MediaUrl { get; set; }

        /// <summary>
        /// Gets or sets Url.
        /// </summary>
        [JsonProperty("url")]
        public string Url { get; set; }
    }
}